/**
 * @file aee.c
 * @author Mathis Rosenhauer, Deutsches Klimarechenzentrum
 * @section DESCRIPTION
 *
 * Adaptive Entropy Encoder
 * Based on CCSDS documents 121.0-B-1 and 120.0-G-2
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#include "libae.h"
#include "aee.h"
#include "aee_mutators.h"

/* Marker for Remainder Of Segment condition in zero block encoding */
#define ROS -1

#define MIN(a, b) (((a) < (b))? (a): (b))

static int m_get_block(ae_streamp strm);
static int m_get_block_cautious(ae_streamp strm);
static int m_check_zero_block(ae_streamp strm);
static int m_select_code_option(ae_streamp strm);
static int m_flush_block(ae_streamp strm);
static int m_flush_block_cautious(ae_streamp strm);
static int m_encode_splitting(ae_streamp strm);
static int m_encode_uncomp(ae_streamp strm);
static int m_encode_se(ae_streamp strm);
static int m_encode_zero(ae_streamp strm);

/*
 *
 * Bit emitters
 *
 */

static inline void emit(encode_state *state, uint32_t data, int bits)
{
    if (bits <= state->bit_p)
    {
        state->bit_p -= bits;
        *state->cds_p += data << state->bit_p;
    }
    else
    {
        bits -= state->bit_p;
        *state->cds_p++ += (uint64_t)data >> bits;

        while (bits & ~7)
        {
            bits -= 8;
            *state->cds_p++ = data >> bits;
        }

        state->bit_p = 8 - bits;
        *state->cds_p = data << state->bit_p;
    }
}

static inline void emitfs(encode_state *state, int fs)
{
    /**
       Emits a fundamental sequence.

       fs zero bits followed by one 1 bit.
     */

    for(;;)
    {
        if (fs < state->bit_p)
        {
            state->bit_p -= fs + 1;
            *state->cds_p += 1 << state->bit_p;
            break;
        }
        else
        {
            fs -= state->bit_p;
            *++state->cds_p = 0;
            state->bit_p = 8;
        }
    }
}

static inline void emitblock(ae_streamp strm, int k, int skip)
{
    int i;
    uint64_t acc;
    encode_state *state = strm->state;
    uint32_t *in = state->block_p + skip;
    uint32_t *in_end = state->block_p + strm->block_size;
    uint64_t mask = (1ULL << k) - 1;
    uint8_t *out = state->cds_p;

    acc = *out;

    while(in < in_end)
    {
        acc <<= 56;
        state->bit_p = (state->bit_p % 8) + 56;

        while (state->bit_p > k && in < in_end)
        {
            state->bit_p -= k;
            acc += ((uint64_t)(*in++) & mask) << state->bit_p;
        }

        for (i = 56; i > (state->bit_p & ~7); i -= 8)
            *out++ = acc >> i;
        acc >>= i;
    }

    *out = acc;
    state->cds_p = out;
    state->bit_p %= 8;
}

static inline void preprocess(ae_streamp strm)
{
    int i;
    int64_t theta, Delta, last_in;
    encode_state *state = strm->state;

    /* Insert reference samples into first block of Reference Sample
     * Interval.
     */
    last_in = state->block_buf[0];

    for (i = 1; i < strm->rsi * strm->block_size; i++)
    {
        theta = MIN(last_in - state->xmin,
                    state->xmax - last_in);
        Delta = (int64_t)state->block_buf[i] - last_in;
        last_in = state->block_buf[i];
        if (0 <= Delta && Delta <= theta)
        {
            state->block_buf[i] = 2 * Delta;
        }
        else if (-theta <= Delta && Delta < 0)
        {
            state->block_buf[i] = 2
                * (Delta < 0 ? -(uint64_t)Delta : Delta) - 1;
        }
        else
        {
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

static int m_get_block(ae_streamp strm)
{
    encode_state *state = strm->state;

    if (strm->avail_out > state->cds_len)
    {
        if (!state->direct_out)
        {
            state->direct_out = 1;
            *strm->next_out = *state->cds_p;
            state->cds_p = strm->next_out;
        }
    }
    else
    {
        if (state->zero_blocks == 0 || state->direct_out)
        {
            /* copy leftover from last block */
            *state->cds_buf = *state->cds_p;
            state->cds_p = state->cds_buf;
        }
        state->direct_out = 0;
    }

    if (state->blocks_avail == 0)
    {
        state->ref = 1;
        state->blocks_avail = strm->rsi - 1;
        state->block_p = state->block_buf;

        if (strm->avail_in >= state->block_len * strm->rsi)
        {
            state->get_block(strm);

            if (strm->flags & AE_DATA_PREPROCESS)
                preprocess(strm);

            return m_check_zero_block(strm);
        }
        else
        {
            state->i = 0;
            state->mode = m_get_block_cautious;
        }
    }
    else
    {
        state->ref = 0;
        state->block_p += strm->block_size;
        state->blocks_avail--;
        return m_check_zero_block(strm);
    }
    return M_CONTINUE;
}

static int m_get_block_cautious(ae_streamp strm)
{
    int j;
    encode_state *state = strm->state;

    do
    {
        if (strm->avail_in > 0)
        {
            state->block_buf[state->i] = state->get_sample(strm);
        }
        else
        {
            if (state->flush == AE_FLUSH)
            {
                if (state->i > 0)
                {
                    /* Pad block with last sample if we have a partial
                     * block.
                     */
                    for (j = state->i; j < strm->rsi * strm->block_size; j++)
                        state->block_buf[j] = state->block_buf[state->i - 1];
                    state->i = strm->rsi * strm->block_size;
                }
                else
                {
                    if (state->zero_blocks)
                    {
                        /* Output any remaining zero blocks */
                        state->mode = m_encode_zero;
                        return M_CONTINUE;
                    }

                    /* Pad last output byte with 0 bits if user wants
                     * to flush, i.e. we got all input there is.
                     */
                    emit(state, 0, state->bit_p);
                    if (state->direct_out == 0)
                        *strm->next_out++ = *state->cds_p;
                    strm->avail_out--;
                    strm->total_out++;

                    return M_EXIT;
                }
            }
            else
            {
                return M_EXIT;
            }
        }
    }
    while (++state->i < strm->rsi * strm->block_size);

    if (strm->flags & AE_DATA_PREPROCESS)
        preprocess(strm);

    return m_check_zero_block(strm);
}

static inline int m_check_zero_block(ae_streamp strm)
{
    int i;
    encode_state *state = strm->state;

    i = state->ref;
    while(i < strm->block_size && state->block_p[i] == 0)
        i++;

    if (i == strm->block_size)
    {
        /* remember ref on first zero block */
        if (state->zero_blocks == 0)
        {
            state->zero_ref = state->ref;
            state->zero_ref_sample = state->block_p[0];
        }

        state->zero_blocks++;

        if ((strm->rsi - state->blocks_avail) % 64 == 0)
        {
            if (state->zero_blocks > 4)
                state->zero_blocks = ROS;
            state->mode = m_encode_zero;
            return M_CONTINUE;
        }
        state->mode = m_get_block;
        return M_CONTINUE;
    }
    else if (state->zero_blocks)
    {
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

static inline int m_select_code_option(ae_streamp strm)
{
    int i, j, k, this_bs, looked_bothways, direction;
    int64_t split_len, uncomp_len;
    int64_t split_len_min, fs_len;
    int64_t d, se_len;
    encode_state *state = strm->state;

    /* Length of this block minus reference sample (if present) */
    this_bs = strm->block_size - state->ref;

    split_len_min = INT64_MAX;
    i = state->k;
    direction = 1;
    looked_bothways = 0;

    /* Starting with splitting position of last block look left and
     * possibly right to find new minimum.
     */
    for (;;)
    {
        fs_len = (int64_t)(state->block_p[1] >> i)
            + (int64_t)(state->block_p[2] >> i)
            + (int64_t)(state->block_p[3] >> i)
            + (int64_t)(state->block_p[4] >> i)
            + (int64_t)(state->block_p[5] >> i)
            + (int64_t)(state->block_p[6] >> i)
            + (int64_t)(state->block_p[7] >> i);

        if (state->ref == 0)
            fs_len += (int64_t)(state->block_p[0] >> i);

        if (strm->block_size > 8)
            for (j = 8; j < strm->block_size; j++)
                fs_len += (int64_t)(state->block_p[j] >> i);

        split_len = fs_len + this_bs * (i + 1);

        if (split_len < split_len_min)
        {
            if (split_len_min < INT64_MAX)
            {
                /* We are moving towards the minimum so it cant be in
                 * the other direction.
                 */
                looked_bothways = 1;
            }
            split_len_min = split_len;
            k = i;

            if (direction == 1)
            {
                if (fs_len < this_bs)
                {
                    /* Next can't get better because what we lose by
                     * additional uncompressed bits isn't compensated
                     * by a smaller FS part. Vice versa if we are
                     * coming from the other direction.
                     */
                    if (looked_bothways)
                    {
                        break;
                    }
                    else
                    {
                        direction = -direction;
                        looked_bothways = 1;
                        i = state->k;
                    }
                }
                else
                {
                    while (fs_len > 5 * this_bs)
                    {
                        i++;
                        fs_len /= 5;
                    }
                }
            }
            else if (fs_len > this_bs)
            {
                /* Since we started looking the other way there is no
                 * need to turn back.
                 */
                break;
            }
        }
        else
        {
            /* Stop looking for better option if we don't see any
             * improvement.
             */
                if (looked_bothways)
                {
                    break;
                }
                else
                {
                    direction = -direction;
                    looked_bothways = 1;
                    i = state->k;
                }
        }
        if (i + direction < 0
            || i + direction >= strm->bit_per_sample - 2)
        {
            if (looked_bothways)
                break;

            direction = -direction;
            looked_bothways = 1;
            i = state->k;
        }

        i += direction;
    }
    state->k = k;

    /* Count bits for 2nd extension */
    se_len = 1;
    for (i = 0; i < strm->block_size; i+= 2)
    {
        d = (int64_t)state->block_p[i] + (int64_t)state->block_p[i + 1];
        /* we have to worry about overflow here */
        if (d > split_len_min)
        {
            se_len = INT64_MAX;
            break;
        }
        else
        {
            se_len += d * (d + 1) / 2 + (int64_t)state->block_p[i + 1];
        }
    }

    /* Length of uncompressed block */
    uncomp_len = this_bs * strm->bit_per_sample;

    /* Decide which option to use */
    if (split_len_min < uncomp_len)
    {
        if (split_len_min < se_len)
        {
            /* Splitting won - the most common case. */
            return m_encode_splitting(strm);
        }
        else
        {
            return m_encode_se(strm);
        }
    }
    else
    {
        if (uncomp_len <= se_len)
        {
            return m_encode_uncomp(strm);
        }
        else
        {
            return m_encode_se(strm);
        }
    }
}

static inline int m_encode_splitting(ae_streamp strm)
{
    int i;
    encode_state *state = strm->state;
    int k = state->k;

    emit(state, k + 1, state->id_len);

    if (state->ref)
        emit(state, state->block_p[0], strm->bit_per_sample);

    for (i = state->ref; i < strm->block_size; i++)
        emitfs(state, state->block_p[i] >> k);

    if (k)
        emitblock(strm, k, state->ref);

    return m_flush_block(strm);
}

static inline int m_encode_uncomp(ae_streamp strm)
{
    encode_state *state = strm->state;

    emit(state, (1 << state->id_len) - 1, state->id_len);
    emitblock(strm, strm->bit_per_sample, 0);

    return m_flush_block(strm);
}

static inline int m_encode_se(ae_streamp strm)
{
    int i;
    uint32_t d;
    encode_state *state = strm->state;

    emit(state, 1, state->id_len + 1);
    if (state->ref)
        emit(state, state->block_p[0], strm->bit_per_sample);

    for (i = 0; i < strm->block_size; i+= 2)
    {
        d = state->block_p[i] + state->block_p[i + 1];
        emitfs(state, d * (d + 1) / 2 + state->block_p[i + 1]);
    }

    return m_flush_block(strm);
}

static inline int m_encode_zero(ae_streamp strm)
{
    encode_state *state = strm->state;

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

static inline int m_flush_block(ae_streamp strm)
{
    int n;
    encode_state *state = strm->state;

    if (state->direct_out)
    {
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

static inline int m_flush_block_cautious(ae_streamp strm)
{
    encode_state *state = strm->state;

    /* Slow restartable flushing */
    while(state->cds_buf + state->i < state->cds_p)
    {
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

int ae_encode_init(ae_streamp strm)
{
    encode_state *state;

    /* Some sanity checks */
    if (strm->bit_per_sample > 32 || strm->bit_per_sample == 0)
        return AE_ERRNO;

    if (strm->block_size != 8
        && strm->block_size != 16
        && strm->block_size != 32
        && strm->block_size != 64)
        return AE_ERRNO;

    if (strm->rsi > 4096)
        return AE_ERRNO;

    /* Internal state for encoder */
    state = (encode_state *) malloc(sizeof(encode_state));
    if (state == NULL)
    {
        return AE_MEM_ERROR;
    }
    memset(state, 0, sizeof(encode_state));
    strm->state = state;

    if (strm->bit_per_sample > 16)
    {
        /* 32 bit settings */
        state->id_len = 5;
        state->block_len = 4 * strm->block_size;

        if (strm->flags & AE_DATA_MSB)
        {
            state->get_sample = get_msb_32;
            state->get_block = get_block_msb_32;
        }
        else
            state->get_sample = get_lsb_32;
    }
    else if (strm->bit_per_sample > 8)
    {
        /* 16 bit settings */
        state->id_len = 4;
        state->block_len = 2 * strm->block_size;

        if (strm->flags & AE_DATA_MSB)
        {
            state->get_sample = get_msb_16;

            if (strm->block_size == 8)
                state->get_block = get_block_msb_16_bs_8;
            else
                state->get_block = get_block_msb_16;
        }
        else
            state->get_sample = get_lsb_16;
    }
    else
    {
        /* 8 bit settings */
        state->block_len = strm->block_size;
        state->id_len = 3;

        state->get_sample = get_8;

        if (strm->block_size == 8)
            state->get_block = get_block_8_bs_8;
        else
            state->get_block = get_block_8;
    }

    if (strm->flags & AE_DATA_SIGNED)
    {
        state->xmin = -(1ULL << (strm->bit_per_sample - 1));
        state->xmax = (1ULL << (strm->bit_per_sample - 1)) - 1;
    }
    else
    {
        state->xmin = 0;
        state->xmax = (1ULL << strm->bit_per_sample) - 1;
    }

    state->block_buf = (uint32_t *)malloc(strm->rsi
                                         * strm->block_size
                                         * sizeof(uint32_t));
    if (state->block_buf == NULL)
    {
        return AE_MEM_ERROR;
    }
    state->block_p = state->block_buf;

    /* Largest possible block according to specs */
    state->cds_len = (5 + 64 * 32) / 8 + 3;
    state->cds_buf = (uint8_t *)malloc(state->cds_len);
    if (state->cds_buf == NULL)
    {
        return AE_MEM_ERROR;
    }

    strm->total_in = 0;
    strm->total_out = 0;

    state->cds_p = state->cds_buf;
    *state->cds_p = 0;
    state->bit_p = 8;
    state->mode = m_get_block;

    return AE_OK;
}

int ae_encode(ae_streamp strm, int flush)
{
    /**
       Finite-state machine implementation of the adaptive entropy
       encoder.
    */
    int n;
    encode_state *state;
    state = strm->state;
    state->flush = flush;

    while (state->mode(strm) == M_CONTINUE);

    if (state->direct_out)
    {
        n = state->cds_p - strm->next_out;
        strm->next_out += n;
        strm->avail_out -= n;
        strm->total_out += n;

        *state->cds_buf = *state->cds_p;
        state->cds_p = state->cds_buf;
        state->direct_out = 0;
    }
    return AE_OK;
}

int ae_encode_end(ae_streamp strm)
{
    encode_state *state = strm->state;

    free(state->block_buf);
    free(state->cds_buf);
    free(state);
    return AE_OK;
}
