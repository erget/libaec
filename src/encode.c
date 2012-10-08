/**
 * @file encode.c
 * @author Mathis Rosenhauer, Deutsches Klimarechenzentrum
 * @section DESCRIPTION
 *
 * Adaptive Entropy Encoder
 * Based on CCSDS documents 121.0-B-2 and 120.0-G-2
 *
 */

#include <config.h>

#if HAVE_STDINT_H
# include <stdint.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "libaec.h"
#include "encode.h"
#include "encode_accessors.h"

/* Marker for Remainder Of Segment condition in zero block encoding */
#define ROS -1

static int m_get_block(struct aec_stream *strm);
static int m_get_rsi_resumable(struct aec_stream *strm);
static int m_check_zero_block(struct aec_stream *strm);
static int m_select_code_option(struct aec_stream *strm);
static int m_flush_block(struct aec_stream *strm);
static int m_flush_block_resumable(struct aec_stream *strm);
static int m_encode_splitting(struct aec_stream *strm);
static int m_encode_uncomp(struct aec_stream *strm);
static int m_encode_se(struct aec_stream *strm);
static int m_encode_zero(struct aec_stream *strm);

static inline void emit(struct internal_state *state,
                        uint32_t data, int bits)
{
    /**
       Emit sequence of bits.
     */

    if (bits <= state->bits) {
        state->bits -= bits;
        *state->cds += data << state->bits;
    } else {
        bits -= state->bits;
        *state->cds++ += (uint64_t)data >> bits;

        while (bits & ~7) {
            bits -= 8;
            *state->cds++ = data >> bits;
        }

        state->bits = 8 - bits;
        *state->cds = data << state->bits;
    }
}

static inline void emitfs(struct internal_state *state, int fs)
{
    /**
       Emits a fundamental sequence.

       fs zero bits followed by one 1 bit.
     */

    for(;;) {
        if (fs < state->bits) {
            state->bits -= fs + 1;
            *state->cds += 1U << state->bits;
            break;
        } else {
            fs -= state->bits;
            *++state->cds = 0;
            state->bits = 8;
        }
    }
}

#define EMITBLOCK(ref)                                          \
    static inline void emitblock_##ref(struct aec_stream *strm, \
                                       int k)                   \
    {                                                           \
        /**                                                     \
           Emit the k LSB of a whole block of input data.       \
        */                                                      \
                                                                \
        int b;                                                  \
        uint64_t a;                                             \
        struct internal_state *state = strm->state;             \
        uint32_t *in = state->block + ref;                      \
        uint32_t *in_end = state->block + strm->block_size;     \
        uint64_t mask = (1ULL << k) - 1;                        \
        uint8_t *o = state->cds;                                \
        int p = state->bits;                                    \
                                                                \
        a = *o;                                                 \
                                                                \
        while(in < in_end) {                                    \
            a <<= 56;                                           \
            p = (p % 8) + 56;                                   \
                                                                \
            while (p > k && in < in_end) {                      \
                p -= k;                                         \
                a += ((uint64_t)(*in++) & mask) << p;           \
            }                                                   \
                                                                \
            for (b = 56; b > (p & ~7); b -= 8)                  \
                *o++ = a >> b;                                  \
            a >>= b;                                            \
        }                                                       \
                                                                \
        *o = a;                                                 \
        state->cds = o;                                         \
        state->bits = p % 8;                                    \
    }

EMITBLOCK(0);
EMITBLOCK(1);

static void preprocess_unsigned(struct aec_stream *strm)
{
    /**
       Preprocess RSI of unsigned samples.
    */

    int64_t D;
    struct internal_state *state = strm->state;
    const uint32_t *x = state->data_raw;
    uint32_t *d = state->data_pp;
    uint32_t xmax = state->xmax;
    uint32_t rsi = strm->rsi * strm->block_size - 1;

    *d++ = x[0];
    while (rsi--) {
        if (x[1] >= x[0]) {
            D = x[1] - x[0];
            if (D <= x[0]) {
                *d = 2 * D;
            } else {
                *d = x[1];
            }
        } else {
            D = x[0] - x[1];
            if (D <= xmax - x[0]) {
                *d = 2 * D - 1;
            } else {
                *d = xmax - x[1];
            }
        }
        d++;
        x++;
    }
}

static void preprocess_signed(struct aec_stream *strm)
{
    /**
       Preprocess RSI of signed samples.
    */

    int64_t D;
    struct internal_state *state = strm->state;
    uint32_t *d = state->data_pp;
    int32_t *x = (int32_t *)state->data_raw;
    uint64_t m = 1ULL << (strm->bit_per_sample - 1);
    int64_t xmax = state->xmax;
    int64_t xmin = state->xmin;
    uint32_t rsi = strm->rsi * strm->block_size - 1;

    *d++ = (uint32_t)x[0];
    x[0] = (x[0] ^ m) - m;

    while (rsi--) {
        x[1] = (x[1] ^ m) - m;
        if (x[1] < x[0]) {
            D = (int64_t)x[0] - x[1];
            if (D <= xmax - x[0])
                *d = 2 * D - 1;
            else
                *d = xmax - x[1];
        } else {
            D = (int64_t)x[1] - x[0];
            if (D <= x[0] - xmin)
                *d = 2 * D;
            else
                *d = x[1] - xmin;
        }
        x++;
        d++;
    }
}

static uint64_t block_fs(struct aec_stream *strm, int k)
{
    /**
       Sum FS of all samples in block for given splitting position.
    */

    int j;
    uint64_t fs;
    struct internal_state *state = strm->state;

    fs = (uint64_t)(state->block[1] >> k)
        + (uint64_t)(state->block[2] >> k)
        + (uint64_t)(state->block[3] >> k)
        + (uint64_t)(state->block[4] >> k)
        + (uint64_t)(state->block[5] >> k)
        + (uint64_t)(state->block[6] >> k)
        + (uint64_t)(state->block[7] >> k);

    if (strm->block_size > 8)
        for (j = 8; j < strm->block_size; j += 8)
            fs +=
                (uint64_t)(state->block[j + 0] >> k)
                + (uint64_t)(state->block[j + 1] >> k)
                + (uint64_t)(state->block[j + 2] >> k)
                + (uint64_t)(state->block[j + 3] >> k)
                + (uint64_t)(state->block[j + 4] >> k)
                + (uint64_t)(state->block[j + 5] >> k)
                + (uint64_t)(state->block[j + 6] >> k)
                + (uint64_t)(state->block[j + 7] >> k);

    if (!state->ref)
        fs += (uint64_t)(state->block[0] >> k);

    return fs;
}

static int assess_splitting_option(struct aec_stream *strm)
{
    /**
       Length of CDS encoded with splitting option and optimal k.

       In Rice coding each sample in a block of samples is split at
       the same position into k LSB and bit_per_sample - k MSB. The
       LSB part is left binary and the MSB part is coded as a
       fundamental sequence a.k.a. unary (see CCSDS 121.0-B-2). The
       function of the length of the Coded Data Set (CDS) depending on
       k has exactly one minimum (see A. Kiely, IPN Progress Report
       42-159).

       To find that minimum with only a few costly evaluations of the
       CDS length, we start with the k of the previous CDS. K is
       increased and the CDS length evaluated. If the CDS length gets
       smaller, then we are moving towards the minimum. If the length
       increases, then the minimum will be found with smaller k.

       For increasing k we know that we will gain block_size bits in
       length through the larger binary part. If the FS lenth is less
       than the block size then a reduced FS part can't compensate the
       larger binary part. So we know that the CDS for k+1 will be
       larger than for k without actually computing the length. An
       analogue check can be done for decreasing k.
     */

    int k;
    int k_min;
    int this_bs; /* Block size of current block */
    int no_turn; /* 1 if we shouldn't reverse */
    int dir; /* Direction, 1 means increasing k, 0 decreasing k */
    uint64_t len; /* CDS length for current k */
    uint64_t len_min; /* CDS length minimum so far */
    uint64_t fs_len; /* Length of FS part (not including 1s) */

    struct internal_state *state = strm->state;

    this_bs = strm->block_size - state->ref;
    len_min = UINT64_MAX;
    k = k_min = state->k;
    no_turn = (k == 0) ? 1 : 0;
    dir = 1;

    for (;;) {
        fs_len = block_fs(strm, k);
        len = fs_len + this_bs * (k + 1);

        if (len < len_min) {
            if (len_min < UINT64_MAX)
                no_turn = 1;

            len_min = len;
            k_min = k;

            if (dir) {
                if (fs_len < this_bs || k >= state->kmax) {
                    if (no_turn)
                        break;
                    k = state->k - 1;
                    dir = 0;
                    no_turn = 1;
                } else {
                    k++;
                }
            } else {
                if (fs_len >= this_bs || k == 0)
                    break;
                k--;
            }
        } else {
            if (no_turn)
                break;
            k = state->k - 1;
            dir = 0;
            no_turn = 1;
        }
    }
    state->k = k_min;

    return len_min;
}

static int assess_se_option(uint64_t limit, struct aec_stream *strm)
{
    /**
       Length of CDS encoded with Second Extension option.

       If length is above limit just return UINT64_MAX.
    */

    int i;
    uint64_t d;
    uint64_t len;
    struct internal_state *state = strm->state;

    len = 1;

    for (i = 0; i < strm->block_size; i+= 2) {
        d = (uint64_t)state->block[i]
            + (uint64_t)state->block[i + 1];
        /* we have to worry about overflow here */
        if (d > limit) {
            len = UINT64_MAX;
            break;
        } else {
            len += d * (d + 1) / 2
                + (uint64_t)state->block[i + 1];
        }
    }
    return len;
}

/*
 *
 * FSM functions
 *
 */

static int m_get_block(struct aec_stream *strm)
{
    /**
       Provide the next block of preprocessed input data.

       Pull in a whole Reference Sample Interval (RSI) of data if
       block buffer is empty.
    */

    struct internal_state *state = strm->state;

    if (strm->avail_out > state->cds_len) {
        if (!state->direct_out) {
            state->direct_out = 1;
            *strm->next_out = *state->cds;
            state->cds = strm->next_out;
        }
    } else {
        if (state->zero_blocks == 0 || state->direct_out) {
            /* copy leftover from last block */
            *state->cds_buf = *state->cds;
            state->cds = state->cds_buf;
        }
        state->direct_out = 0;
    }

    if (state->blocks_avail == 0) {
        state->block = state->data_pp;

        if (strm->avail_in >= state->block_len * strm->rsi) {
            state->get_rsi(strm);
            state->blocks_avail = strm->rsi - 1;

            if (strm->flags & AEC_DATA_PREPROCESS) {
                state->preprocess(strm);
                state->ref = 1;
            }
            return m_check_zero_block(strm);
        } else {
            state->i = 0;
            state->mode = m_get_rsi_resumable;
        }
    } else {
        state->ref = 0;
        state->block += strm->block_size;
        state->blocks_avail--;
        return m_check_zero_block(strm);
    }
    return M_CONTINUE;
}

static int m_get_rsi_resumable(struct aec_stream *strm)
{
    /**
       Get RSI while input buffer is short.

       Let user provide more input. Once we got all input pad buffer
       to full RSI.
    */

    int j;
    struct internal_state *state = strm->state;

    do {
        if (strm->avail_in > 0) {
            state->data_raw[state->i] = state->get_sample(strm);
        } else {
            if (state->flush == AEC_FLUSH) {
                if (state->i > 0) {
                    for (j = state->i; j < strm->rsi * strm->block_size; j++)
                        state->data_raw[j] = state->data_raw[state->i - 1];
                    state->i = strm->rsi * strm->block_size;
                } else {
                    if (state->zero_blocks) {
                        state->mode = m_encode_zero;
                        return M_CONTINUE;
                    }

                    emit(state, 0, state->bits);
                    if (state->direct_out == 0)
                        *strm->next_out++ = *state->cds;
                    strm->avail_out--;
                    strm->total_out++;

                    return M_EXIT;
                }
            } else {
                return M_EXIT;
            }
        }
    } while (++state->i < strm->rsi * strm->block_size);

    state->blocks_avail = strm->rsi - 1;
    if (strm->flags & AEC_DATA_PREPROCESS) {
        state->preprocess(strm);
        state->ref = 1;
    }

    return m_check_zero_block(strm);
}

static int m_check_zero_block(struct aec_stream *strm)
{
    /**
       Check if input block is all zero.

       Aggregate consecutive zero blocks until we find !0 or reach the
       end of a segment or RSI.
    */

    struct internal_state *state = strm->state;
    uint32_t *p = state->block + state->ref;
    uint32_t *end = state->block + strm->block_size;

    while(p < end && *p == 0)
        p++;

    if (p < end) {
        if (state->zero_blocks) {
            /* The current block isn't zero but we have to emit a
             * previous zero block first. The current block will be
             * handled later.
             */
            state->block -= strm->block_size;
            state->blocks_avail++;
            state->mode = m_encode_zero;
            return M_CONTINUE;
        }
        state->mode = m_select_code_option;
        return M_CONTINUE;
    } else {
        state->zero_blocks++;
        if (state->zero_blocks == 1) {
            state->zero_ref = state->ref;
            state->zero_ref_sample = state->block[0];
        }
        if (state->blocks_avail == 0
            || (strm->rsi - state->blocks_avail) % 64 == 0) {
            if (state->zero_blocks > 4)
                state->zero_blocks = ROS;
            state->mode = m_encode_zero;
            return M_CONTINUE;
        }
        state->mode = m_get_block;
        return M_CONTINUE;
    }
}

static int m_select_code_option(struct aec_stream *strm)
{
    /**
       Decide which code option to use.
    */

    uint64_t uncomp_len;
    uint64_t split_len;
    uint64_t se_len;
    struct internal_state *state = strm->state;

    uncomp_len = (strm->block_size - state->ref)
        * strm->bit_per_sample;
    split_len = assess_splitting_option(strm);
    se_len = assess_se_option(split_len, strm);

    if (split_len < uncomp_len) {
        if (split_len < se_len)
            return m_encode_splitting(strm);
        else
            return m_encode_se(strm);
    } else {
        if (uncomp_len <= se_len)
            return m_encode_uncomp(strm);
        else
            return m_encode_se(strm);
    }
}

static int m_encode_splitting(struct aec_stream *strm)
{
    int i;
    struct internal_state *state = strm->state;
    int k = state->k;

    emit(state, k + 1, state->id_len);

    if (state->ref)
    {
        emit(state, state->block[0], strm->bit_per_sample);
        for (i = 1; i < strm->block_size; i++)
            emitfs(state, state->block[i] >> k);
        if (k)
            emitblock_1(strm, k);
    }
    else
    {
        for (i = 0; i < strm->block_size; i++)
            emitfs(state, state->block[i] >> k);
        if (k)
            emitblock_0(strm, k);
    }

    return m_flush_block(strm);
}

static int m_encode_uncomp(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    emit(state, (1U << state->id_len) - 1, state->id_len);
    emitblock_0(strm, strm->bit_per_sample);

    return m_flush_block(strm);
}

static int m_encode_se(struct aec_stream *strm)
{
    int i;
    uint32_t d;
    struct internal_state *state = strm->state;

    emit(state, 1, state->id_len + 1);
    if (state->ref)
        emit(state, state->block[0], strm->bit_per_sample);

    for (i = 0; i < strm->block_size; i+= 2) {
        d = state->block[i] + state->block[i + 1];
        emitfs(state, d * (d + 1) / 2 + state->block[i + 1]);
    }

    return m_flush_block(strm);
}

static int m_encode_zero(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    emit(state, 0, state->id_len + 1);

    if (state->zero_ref)
        emit(state, state->zero_ref_sample, strm->bit_per_sample);

    if (state->zero_blocks == ROS)
        emitfs(state, 4);
    else if (state->zero_blocks >= 5)
        emitfs(state, state->zero_blocks);
    else
        emitfs(state, state->zero_blocks - 1);

    state->zero_blocks = 0;
    return m_flush_block(strm);
}

static int m_flush_block(struct aec_stream *strm)
{
    /**
       Flush block in direct_out mode by updating counters.

       Fall back to slow flushing if in buffered mode.
    */
    int n;
    struct internal_state *state = strm->state;

    if (state->direct_out) {
        n = state->cds - strm->next_out;
        strm->next_out += n;
        strm->avail_out -= n;
        strm->total_out += n;
        state->mode = m_get_block;
        return M_CONTINUE;
    }

    state->i = 0;
    state->mode = m_flush_block_resumable;
    return M_CONTINUE;
}

static int m_flush_block_resumable(struct aec_stream *strm)
{
    /**
       Slow and restartable flushing
    */
    struct internal_state *state = strm->state;

    while(state->cds_buf + state->i < state->cds) {
        if (strm->avail_out == 0)
            return M_EXIT;

        *strm->next_out++ = state->cds_buf[state->i];
        strm->avail_out--;
        strm->total_out++;
        state->i++;
    }
    state->mode = m_get_block;
    return M_CONTINUE;
}

/*
 *
 * API functions
 *
 */

int aec_encode_init(struct aec_stream *strm)
{
    struct internal_state *state;

    if (strm->bit_per_sample > 32 || strm->bit_per_sample == 0)
        return AEC_CONF_ERROR;

    if (strm->block_size != 8
        && strm->block_size != 16
        && strm->block_size != 32
        && strm->block_size != 64)
        return AEC_CONF_ERROR;

    if (strm->rsi > 4096)
        return AEC_CONF_ERROR;

    state = (struct internal_state *)malloc(sizeof(struct internal_state));
    if (state == NULL)
        return AEC_MEM_ERROR;

    memset(state, 0, sizeof(struct internal_state));
    strm->state = state;

    if (strm->bit_per_sample > 16) {
        /* 24/32 input bit settings */
        state->id_len = 5;

        if (strm->bit_per_sample <= 24
            && strm->flags & AEC_DATA_3BYTE) {
            state->block_len = 3 * strm->block_size;
            if (strm->flags & AEC_DATA_MSB) {
                state->get_sample = get_msb_24;
                state->get_rsi = get_rsi_msb_24;
            } else {
                state->get_sample = get_lsb_24;
                state->get_rsi = get_rsi_lsb_24;
            }
        } else {
            state->block_len = 4 * strm->block_size;
            if (strm->flags & AEC_DATA_MSB) {
                state->get_sample = get_msb_32;
                state->get_rsi = get_rsi_msb_32;
            } else {
                state->get_sample = get_lsb_32;
                state->get_rsi = get_rsi_lsb_32;
            }
        }
    }
    else if (strm->bit_per_sample > 8) {
        /* 16 bit settings */
        state->id_len = 4;
        state->block_len = 2 * strm->block_size;

        if (strm->flags & AEC_DATA_MSB) {
            state->get_sample = get_msb_16;
            state->get_rsi = get_rsi_msb_16;
        } else {
            state->get_sample = get_lsb_16;
            state->get_rsi = get_rsi_lsb_16;
        }
    } else {
        /* 8 bit settings */
        state->id_len = 3;
        state->block_len = strm->block_size;

        state->get_sample = get_8;
        state->get_rsi = get_rsi_8;
    }

    if (strm->flags & AEC_DATA_SIGNED) {
        state->xmin = -(1ULL << (strm->bit_per_sample - 1));
        state->xmax = (1ULL << (strm->bit_per_sample - 1)) - 1;
        state->preprocess = preprocess_signed;
    } else {
        state->xmin = 0;
        state->xmax = (1ULL << strm->bit_per_sample) - 1;
        state->preprocess = preprocess_unsigned;
    }

    state->kmax = (1U << state->id_len) - 3;

    state->data_pp = (uint32_t *)malloc(strm->rsi
                                         * strm->block_size
                                         * sizeof(uint32_t));
    if (state->data_pp == NULL)
        return AEC_MEM_ERROR;

    if (strm->flags & AEC_DATA_PREPROCESS) {
        state->data_raw = (uint32_t *)malloc(strm->rsi
                                             * strm->block_size
                                             * sizeof(uint32_t));
        if (state->data_raw == NULL)
            return AEC_MEM_ERROR;
    } else {
        state->data_raw = state->data_pp;
    }

    state->block = state->data_pp;

    /* Largest possible CDS according to specs */
    state->cds_len = (5 + 64 * 32) / 8 + 3;
    state->cds_buf = (uint8_t *)malloc(state->cds_len);
    if (state->cds_buf == NULL)
        return AEC_MEM_ERROR;

    strm->total_in = 0;
    strm->total_out = 0;

    state->cds = state->cds_buf;
    *state->cds = 0;
    state->bits = 8;
    state->mode = m_get_block;

    return AEC_OK;
}

int aec_encode(struct aec_stream *strm, int flush)
{
    /**
       Finite-state machine implementation of the adaptive entropy
       encoder.
    */
    int n;
    struct internal_state *state;
    state = strm->state;
    state->flush = flush;

    while (state->mode(strm) == M_CONTINUE);

    if (state->direct_out) {
        n = state->cds - strm->next_out;
        strm->next_out += n;
        strm->avail_out -= n;
        strm->total_out += n;

        *state->cds_buf = *state->cds;
        state->cds = state->cds_buf;
        state->direct_out = 0;
    }
    return AEC_OK;
}

int aec_encode_end(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    if (strm->flags & AEC_DATA_PREPROCESS)
        free(state->data_raw);
    free(state->data_pp);
    free(state->cds_buf);
    free(state);
    return AEC_OK;
}

int aec_buffer_encode(struct aec_stream *strm)
{
    int status;

    status = aec_encode_init(strm);
    if (status != AEC_OK)
        return status;
    status = aec_encode(strm, AEC_FLUSH);
    if (strm->avail_in > 0 || strm->avail_out == 0)
        status = AEC_DATA_ERROR;

    aec_encode_end(strm);
    return status;
}
