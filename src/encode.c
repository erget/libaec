/**
 * @file encode.c
 * @author Mathis Rosenhauer, Deutsches Klimarechenzentrum
 * @section DESCRIPTION
 *
 * Adaptive Entropy Encoder
 * Based on CCSDS documents 121.0-B-2 and 120.0-G-2
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#include "libaec.h"
#include "encode.h"
#include "encode_accessors.h"

/* Marker for Remainder Of Segment condition in zero block encoding */
#define ROS -1

#define MIN(a, b) (((a) < (b))? (a): (b))

static int m_get_block(struct aec_stream *strm);
static int m_get_block_cautious(struct aec_stream *strm);
static int m_check_zero_block(struct aec_stream *strm);
static int m_select_code_option(struct aec_stream *strm);
static int m_flush_block(struct aec_stream *strm);
static int m_flush_block_cautious(struct aec_stream *strm);
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

    if (bits <= state->bit_p) {
        state->bit_p -= bits;
        *state->cds_p += data << state->bit_p;
    } else {
        bits -= state->bit_p;
        *state->cds_p++ += (uint64_t)data >> bits;

        while (bits & ~7) {
            bits -= 8;
            *state->cds_p++ = data >> bits;
        }

        state->bit_p = 8 - bits;
        *state->cds_p = data << state->bit_p;
    }
}

static inline void emitfs(struct internal_state *state, int fs)
{
    /**
       Emits a fundamental sequence.

       fs zero bits followed by one 1 bit.
     */

    for(;;) {
        if (fs < state->bit_p) {
            state->bit_p -= fs + 1;
            *state->cds_p += 1 << state->bit_p;
            break;
        } else {
            fs -= state->bit_p;
            *++state->cds_p = 0;
            state->bit_p = 8;
        }
    }
}

#define EMITBLOCK(ref)                                          \
    static inline void emitblock_##ref(struct aec_stream *strm, \
                                       int k)                   \
    {                                                           \
        int b;                                                  \
        uint64_t a;                                             \
        struct internal_state *state = strm->state;             \
        uint32_t *in = state->block_p + ref;                    \
        uint32_t *in_end = state->block_p + strm->block_size;   \
        uint64_t mask = (1ULL << k) - 1;                        \
        uint8_t *o = state->cds_p;                              \
        int p = state->bit_p;                                   \
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
        state->cds_p = o;                                       \
        state->bit_p = p % 8;                                   \
    }

EMITBLOCK(0);
EMITBLOCK(1);

static void preprocess_unsigned(struct aec_stream *strm)
{
    int i;
    int64_t theta, Delta, prev;
    struct internal_state *state = strm->state;

    prev = state->block_buf[0];

    for (i = 1; i < strm->rsi * strm->block_size; i++) {
        theta = MIN(prev, state->xmax - prev);
        Delta = (int64_t)state->block_buf[i] - prev;
        prev = state->block_buf[i];

        if (0 <= Delta && Delta <= theta) {
            state->block_buf[i] = 2 * Delta;
        } else if (-theta <= Delta && Delta < 0) {
            state->block_buf[i] = 2
                * (Delta < 0 ? -(uint64_t)Delta : Delta) - 1;
        } else {
            state->block_buf[i] = theta
                + (Delta < 0 ? -(uint64_t)Delta : Delta);
        }
    }
}

static void preprocess_signed(struct aec_stream *strm)
{
    int i, m;
    int64_t theta, Delta, prev, sample;
    struct internal_state *state = strm->state;

    m = 64 - strm->bit_per_sample;
    prev = ((int64_t)state->block_buf[0] << m) >> m;

    for (i = 1; i < strm->rsi * strm->block_size; i++) {
        theta = MIN(prev - state->xmin, state->xmax - prev);
        sample = ((int64_t)state->block_buf[i] << m) >> m;
        Delta = sample - prev;
        prev = sample;

        if (0 <= Delta && Delta <= theta) {
            state->block_buf[i] = 2 * Delta;
        } else if (-theta <= Delta && Delta < 0) {
            state->block_buf[i] = 2
                * (Delta < 0 ? -(uint64_t)Delta : Delta) - 1;
        } else {
            state->block_buf[i] = theta
                + (Delta < 0 ? -(uint64_t)Delta : Delta);
        }
    }
}

/*
 *
 * FSM functions
 *
 */

static int m_get_block(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    if (strm->avail_out > state->cds_len) {
        if (!state->direct_out) {
            state->direct_out = 1;
            *strm->next_out = *state->cds_p;
            state->cds_p = strm->next_out;
        }
    } else {
        if (state->zero_blocks == 0 || state->direct_out) {
            /* copy leftover from last block */
            *state->cds_buf = *state->cds_p;
            state->cds_p = state->cds_buf;
        }
        state->direct_out = 0;
    }

    if (state->blocks_avail == 0) {
        state->ref = 1;
        state->blocks_avail = strm->rsi - 1;
        state->block_p = state->block_buf;

        if (strm->avail_in >= state->block_len * strm->rsi) {
            state->get_block(strm);

            if (strm->flags & AEC_DATA_PREPROCESS)
                state->preprocess(strm);

            return m_check_zero_block(strm);
        } else {
            state->i = 0;
            state->mode = m_get_block_cautious;
        }
    } else {
        state->ref = 0;
        state->block_p += strm->block_size;
        state->blocks_avail--;
        return m_check_zero_block(strm);
    }
    return M_CONTINUE;
}

static int input_empty(struct aec_stream *strm)
{
    int j;
    struct internal_state *state = strm->state;

    if (state->flush == AEC_FLUSH) {
        if (state->i > 0) {
            for (j = state->i; j < strm->rsi * strm->block_size; j++)
                state->block_buf[j] = state->block_buf[state->i - 1];
            state->i = strm->rsi * strm->block_size;
        } else {
            if (state->zero_blocks) {
                state->mode = m_encode_zero;
                return M_CONTINUE;
            }

            emit(state, 0, state->bit_p);
            if (state->direct_out == 0)
                *strm->next_out++ = *state->cds_p;
            strm->avail_out--;
            strm->total_out++;

            return M_EXIT;
        }
    }

    return M_EXIT;
}

static int m_get_block_cautious(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    do {
        if (strm->avail_in > 0)
            state->block_buf[state->i] = state->get_sample(strm);
        else
            return input_empty(strm);
    } while (++state->i < strm->rsi * strm->block_size);

    if (strm->flags & AEC_DATA_PREPROCESS)
        state->preprocess(strm);

    return m_check_zero_block(strm);
}

static int m_check_zero_block(struct aec_stream *strm)
{
    int i;
    struct internal_state *state = strm->state;

    i = state->ref;
    while(i < strm->block_size && state->block_p[i] == 0)
        i++;

    if (i == strm->block_size) {
        if (state->zero_blocks == 0) {
            state->zero_ref = state->ref;
            state->zero_ref_sample = state->block_p[0];
        }

        state->zero_blocks++;

        if ((strm->rsi - state->blocks_avail) % 64 == 0) {
            if (state->zero_blocks > 4)
                state->zero_blocks = ROS;
            state->mode = m_encode_zero;
            return M_CONTINUE;
        }
        state->mode = m_get_block;
        return M_CONTINUE;
    } else if (state->zero_blocks) {
        /* The current block isn't zero but we have to emit a previous
         * zero block first. The current block will be handled
         * later.
         */
        state->block_p -= strm->block_size;
        state->blocks_avail++;
        state->mode = m_encode_zero;
        return M_CONTINUE;
    }
    state->mode = m_select_code_option;
    return M_CONTINUE;
}

static uint64_t block_fs(struct aec_stream *strm, int k)
{
    int j;
    uint64_t fs;
    struct internal_state *state = strm->state;

    fs = (uint64_t)(state->block_p[1] >> k)
        + (uint64_t)(state->block_p[2] >> k)
        + (uint64_t)(state->block_p[3] >> k)
        + (uint64_t)(state->block_p[4] >> k)
        + (uint64_t)(state->block_p[5] >> k)
        + (uint64_t)(state->block_p[6] >> k)
        + (uint64_t)(state->block_p[7] >> k);

    if (strm->block_size > 8)
        for (j = 1; j < strm->block_size / 8; j++)
            fs +=
                (uint64_t)(state->block_p[j * 8 + 0] >> k)
                + (uint64_t)(state->block_p[j * 8 + 1] >> k)
                + (uint64_t)(state->block_p[j * 8 + 2] >> k)
                + (uint64_t)(state->block_p[j * 8 + 3] >> k)
                + (uint64_t)(state->block_p[j * 8 + 4] >> k)
                + (uint64_t)(state->block_p[j * 8 + 5] >> k)
                + (uint64_t)(state->block_p[j * 8 + 6] >> k)
                + (uint64_t)(state->block_p[j * 8 + 7] >> k);

    if (state->ref == 0)
        fs += (uint64_t)(state->block_p[0] >> k);

    return fs;
}

static int count_splitting_option(struct aec_stream *strm)
{
    int i, k, this_bs, looked_bothways, direction;
    uint64_t len, len_min, fs_len;
    struct internal_state *state = strm->state;

    this_bs = strm->block_size - state->ref;
    len_min = UINT64_MAX;
    i = k = state->k;
    direction = 1;
    looked_bothways = 0;

    /* Starting with splitting position of last block. Look left and
     * possibly right to find new minimum.
     */
    for (;;) {
        fs_len = block_fs(strm, i);
        len = fs_len + this_bs * (i + 1);

        if (len < len_min) {
            if (len_min < UINT64_MAX) {
                /* We are moving towards the minimum so it cant be in
                 * the other direction.
                 */
                looked_bothways = 1;
            }
            len_min = len;
            k = i;

            if (direction == 1) {
                if (fs_len < this_bs) {
                    /* Next can't get better because what we lose by
                     * additional uncompressed bits isn't compensated
                     * by a smaller FS part. Vice versa if we are
                     * coming from the other direction.
                     */
                    if (looked_bothways) {
                        break;
                    } else {
                        direction = -direction;
                        looked_bothways = 1;
                        i = state->k;
                    }
                } else {
                    while (fs_len > 5 * this_bs) {
                        i++;
                        fs_len /= 5;
                    }
                }
            } else if (fs_len > this_bs) {
                /* Since we started looking the other way there is no
                 * need to turn back.
                 */
                break;
            }
        } else {
            /* Stop looking for better option if we don't see any
             * improvement.
             */
            if (looked_bothways) {
                break;
            } else {
                direction = -direction;
                looked_bothways = 1;
                i = state->k;
            }
        }
        if (i + direction < 0
            || i + direction >= strm->bit_per_sample - 2) {
            if (looked_bothways)
                break;

            direction = -direction;
            looked_bothways = 1;
            i = state->k;
        }
        i += direction;
    }
    state->k = k;

    return len_min;
}

static int count_se_option(uint64_t limit, struct aec_stream *strm)
{
    int i;
    uint64_t d, len;
    struct internal_state *state = strm->state;

    len = 1;

    for (i = 0; i < strm->block_size; i+= 2) {
        d = (uint64_t)state->block_p[i]
            + (uint64_t)state->block_p[i + 1];
        /* we have to worry about overflow here */
        if (d > limit) {
            len = UINT64_MAX;
            break;
        } else {
            len += d * (d + 1) / 2
                + (uint64_t)state->block_p[i + 1];
        }
    }
    return len;
}

static int m_select_code_option(struct aec_stream *strm)
{
    uint64_t uncomp_len, split_len, se_len;
    struct internal_state *state = strm->state;

    uncomp_len = (strm->block_size - state->ref)
        * strm->bit_per_sample;
    split_len = count_splitting_option(strm);
    se_len = count_se_option(split_len, strm);

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
        emit(state, state->block_p[0], strm->bit_per_sample);
        for (i = 1; i < strm->block_size; i++)
            emitfs(state, state->block_p[i] >> k);
        if (k) emitblock_1(strm, k);
    }
    else
    {
        for (i = 0; i < strm->block_size; i++)
            emitfs(state, state->block_p[i] >> k);
        if (k) emitblock_0(strm, k);
    }

    return m_flush_block(strm);
}

static int m_encode_uncomp(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    emit(state, (1 << state->id_len) - 1, state->id_len);
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
        emit(state, state->block_p[0], strm->bit_per_sample);

    for (i = 0; i < strm->block_size; i+= 2) {
        d = state->block_p[i] + state->block_p[i + 1];
        emitfs(state, d * (d + 1) / 2 + state->block_p[i + 1]);
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
        n = state->cds_p - strm->next_out;
        strm->next_out += n;
        strm->avail_out -= n;
        strm->total_out += n;
        state->mode = m_get_block;
        return M_CONTINUE;
    }

    state->i = 0;
    state->mode = m_flush_block_cautious;
    return M_CONTINUE;
}

static int m_flush_block_cautious(struct aec_stream *strm)
{
    /**
       Slow and restartable flushing
    */
    struct internal_state *state = strm->state;

    while(state->cds_buf + state->i < state->cds_p) {
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
    int bs, bsi;
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

    bs = strm->block_size >> 3;
    bsi = 0;
    while (bs >>= 1)
        bsi++;

    if (strm->bit_per_sample > 16) {
        /* 24/32 input bit settings */
        state->id_len = 5;

        if (strm->bit_per_sample <= 24
            && strm->flags & AEC_DATA_3BYTE) {
            state->block_len = 3 * strm->block_size;
            if (strm->flags & AEC_DATA_MSB) {
                state->get_sample = get_msb_24;
                state->get_block = get_block_funcs_msb_24[bsi];
            } else {
                state->get_sample = get_lsb_24;
                state->get_block = get_block_funcs_lsb_24[bsi];
            }
        } else {
            state->block_len = 4 * strm->block_size;
            if (strm->flags & AEC_DATA_MSB) {
                state->get_sample = get_msb_32;
                state->get_block = get_block_funcs_msb_32[bsi];
            } else {
                state->get_sample = get_lsb_32;
                state->get_block = get_block_funcs_lsb_32[bsi];
            }
        }
    }
    else if (strm->bit_per_sample > 8) {
        /* 16 bit settings */
        state->id_len = 4;
        state->block_len = 2 * strm->block_size;

        if (strm->flags & AEC_DATA_MSB) {
            state->get_sample = get_msb_16;
            state->get_block = get_block_funcs_msb_16[bsi];
        } else {
            state->get_sample = get_lsb_16;
            state->get_block = get_block_funcs_lsb_16[bsi];
        }
    } else {
        /* 8 bit settings */
        state->id_len = 3;
        state->block_len = strm->block_size;

        state->get_sample = get_8;
        state->get_block = get_block_funcs_8[bsi];
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

    state->block_buf = (uint32_t *)malloc(strm->rsi
                                         * strm->block_size
                                         * sizeof(uint32_t));
    if (state->block_buf == NULL)
        return AEC_MEM_ERROR;

    state->block_p = state->block_buf;

    /* Largest possible CDS according to specs */
    state->cds_len = (5 + 64 * 32) / 8 + 3;
    state->cds_buf = (uint8_t *)malloc(state->cds_len);
    if (state->cds_buf == NULL)
        return AEC_MEM_ERROR;

    strm->total_in = 0;
    strm->total_out = 0;

    state->cds_p = state->cds_buf;
    *state->cds_p = 0;
    state->bit_p = 8;
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
        n = state->cds_p - strm->next_out;
        strm->next_out += n;
        strm->avail_out -= n;
        strm->total_out += n;

        *state->cds_buf = *state->cds_p;
        state->cds_p = state->cds_buf;
        state->direct_out = 0;
    }
    return AEC_OK;
}

int aec_encode_end(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    free(state->block_buf);
    free(state->cds_buf);
    free(state);
    return AEC_OK;
}
