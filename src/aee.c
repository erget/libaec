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
#define MAX(a, b) (((a) > (b))? (a): (b))

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

static inline void emit(encode_state *state, uint64_t data, int bits)
{
    for(;;)
    {
        data &= ((1ULL << bits) - 1);
        if (bits <= state->bitp)
        {
            state->bitp -= bits;
            *state->out_bp += data << state->bitp;
            break;
        }
        else
        {
            bits -= state->bitp;
            *state->out_bp += data >> bits;
            *++state->out_bp = 0;
            state->bitp = 8;
        }
    }
}

static inline void emitfs(encode_state *state, int fs)
{
    /**
       Emits a fundamental sequence.

       fs zero bits followed by one 1 bit.
     */

    fs++;
    for(;;)
    {
        if (fs <= state->bitp)
        {
            state->bitp -= fs;
            *state->out_bp += 1 << state->bitp;
            break;
        }
        else
        {
            fs -= state->bitp;
            *++state->out_bp = 0;
            state->bitp = 8;
        }
    }
}

static inline void preprocess(ae_streamp strm)
{
    int i;
    int64_t theta, d, Delta;
    encode_state *state = strm->state;

    /* If this is the first block between reference 
       samples then we need to insert one.
    */
    if(state->in_total_blocks % strm->rsi == 0)
    {
        state->ref = 1;
        state->last_in = state->in_block[0];
    }
    else
    {
        state->ref = 0;
    }

    for (i = state->ref; i < strm->block_size; i++)
    {
        theta = MIN(state->last_in - state->xmin,
                    state->xmax - state->last_in);
        Delta = state->in_block[i] - state->last_in;
        state->last_in = state->in_block[i];
        if (0 <= Delta && Delta <= theta)
        {
            state->in_block[i] = 2 * Delta;
        }
        else if (-theta <= Delta && Delta < 0)
        {
            d = Delta < 0 ? -(uint64_t)Delta : Delta;
            state->in_block[i] = 2 * d - 1;
        }
        else
        {
            state->in_block[i] = theta +
                (Delta < 0 ? -(uint64_t)Delta : Delta);
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

    if (strm->avail_out > state->out_blklen)
    {
        if (!state->out_direct)
        {
            state->out_direct = 1;
            *strm->next_out = *state->out_bp;
            state->out_bp = strm->next_out;
        }
    }
    else
    {
        if (state->zero_blocks == 0 || state->out_direct)
        {
            /* copy leftover from last block */
            *state->out_block = *state->out_bp;
            state->out_bp = state->out_block;
        }
        state->out_direct = 0;
    }

    if(state->block_deferred)
    {
        state->block_deferred = 0;
        state->mode = m_select_code_option;
        return M_CONTINUE;
    }

    if (strm->avail_in >= state->in_blklen)
    {
        state->get_block(strm);

        if (strm->flags & AE_DATA_PREPROCESS)
            preprocess(strm);

        state->in_total_blocks++;
        return m_check_zero_block(strm);
    }
    else
    {
        state->i = 0;
        state->mode = m_get_block_cautious;
    }
    return M_CONTINUE;
}

static int m_get_block_cautious(ae_streamp strm)
{
    encode_state *state = strm->state;

    do
    {
        if (strm->avail_in == 0)
        {
            if (state->flush == AE_FLUSH)
            {
                if (state->i > 0)
                {
                    /* pad block with last sample if we have
                       a partial block */
                    state->in_block[state->i] = state->in_block[state->i - 1];
                }
                else
                {
                    if (state->zero_blocks)
                    {
                        /* Output any remaining zero blocks */
                        state->mode = m_encode_zero;
                        return M_CONTINUE;
                    }
                    /* Pad last output byte with 0 bits
                       if user wants to flush, i.e. we got
                       all input there is.
                    */
                    emit(state, 0, state->bitp);
                    if (state->out_direct == 0)
                        *strm->next_out++ = *state->out_bp;
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
        else
        {
            state->in_block[state->i] = state->get_sample(strm);
        }
    }
    while (++state->i < strm->block_size);

    if (strm->flags & AE_DATA_PREPROCESS)
        preprocess(strm);

    state->in_total_blocks++;
    return m_check_zero_block(strm);
}

static inline int m_check_zero_block(ae_streamp strm)
{
    int i;
    encode_state *state = strm->state;

    i = state->ref;
    while(i < strm->block_size && state->in_block[i] == 0)
        i++;

    if (i == strm->block_size)
    {
        /* remember ref on first zero block */
        if (state->zero_blocks == 0)
        {
            state->zero_ref = state->ref;
            state->zero_ref_sample = state->in_block[0];
        }

        state->zero_blocks++;

        if (state->in_total_blocks % strm->rsi % 64 == 0)
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
        /* The current block isn't zero but we have to
           emit a previous zero block first. The
           current block will be handled later.
        */
        state->block_deferred = 1;
        state->mode = m_encode_zero;
        return M_CONTINUE;
    }
    state->mode = m_select_code_option;
    return M_CONTINUE;
}

static inline int m_select_code_option(ae_streamp strm)
{
    int i, k, this_bs, looked_bothways, direction;
    int64_t d, split_len, uncomp_len;
    int64_t split_len_min, se_len, fs_len;
    encode_state *state = strm->state;

    /* Length of this block minus reference sample (if present) */
    this_bs = strm->block_size - state->ref;

    split_len_min = INT64_MAX;
    i = state->k;
    direction = 1;
    looked_bothways = 0;

    /* Starting with splitting position of last block look left
       and possibly right to find new minimum.*/
    for (;;)
    {
        fs_len = (state->in_block[1] >> i)
            + (state->in_block[2] >> i)
            + (state->in_block[3] >> i)
            + (state->in_block[4] >> i)
            + (state->in_block[5] >> i)
            + (state->in_block[6] >> i)
            + (state->in_block[7] >> i);

        if (state->ref == 0)
            fs_len += (state->in_block[0] >> i);

        if (strm->block_size == 16)
            fs_len += (state->in_block[8] >> i)
                + (state->in_block[9] >> i)
                + (state->in_block[10] >> i)
                + (state->in_block[11] >> i)
                + (state->in_block[12] >> i)
                + (state->in_block[13] >> i)
                + (state->in_block[14] >> i)
                + (state->in_block[15] >> i);

        split_len = fs_len + this_bs * (i + 1);

        if (split_len < split_len_min)
        {
            if (split_len_min < INT64_MAX)
            {
                /* We are moving towards the minimum so it cant be in
                   the other direction.*/
                looked_bothways = 1;
            }
            split_len_min = split_len;
            k = i;

            if (direction == 1)
            {
                if (fs_len < this_bs)
                {
                    /* Next can't get better because what we lose by
                       additional uncompressed bits isn't compensated by a
                       smaller FS part. Vice versa if we are coming from
                       the other direction.*/
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
                   need to turn back.*/
                break;
            }
        }
        else
        {
            /* Stop looking for better option if we
               don't see any improvement. */
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
        d = state->in_block[i] + state->in_block[i + 1];
        /* we have to worry about overflow here */
        if (d > split_len_min)
        {
            se_len = d;
            break;
        }
        else
        {
            se_len += d * (d + 1) / 2 + state->in_block[i + 1];
        }
    }

    /* Length of uncompressed block */
    uncomp_len = this_bs * strm->bit_per_sample;

    /* Decide which option to use */
    if (split_len_min < uncomp_len)
    {
        if (split_len_min <= se_len)
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
        emit(state, state->in_block[0], strm->bit_per_sample);

    for (i = state->ref; i < strm->block_size; i++)
        emitfs(state, state->in_block[i] >> k);

    for (i = state->ref; i < strm->block_size; i++)
        emit(state, state->in_block[i], k);

    return m_flush_block(strm);
}

static inline int m_encode_uncomp(ae_streamp strm)
{
    int i;
    encode_state *state = strm->state;

    emit(state, 0x1f, state->id_len);
    for (i = 0; i < strm->block_size; i++)
        emit(state, state->in_block[i], strm->bit_per_sample);

    return m_flush_block(strm);
}

static inline int m_encode_se(ae_streamp strm)
{
    int i;
    int64_t d;
    encode_state *state = strm->state;

    emit(state, 1, state->id_len + 1);
    if (state->ref)
        emit(state, state->in_block[0], strm->bit_per_sample);

    for (i = 0; i < strm->block_size; i+= 2)
    {
        d = state->in_block[i] + state->in_block[i + 1];
        emitfs(state, d * (d + 1) / 2 + state->in_block[i + 1]);
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

    if (state->out_direct)
    {
        n = state->out_bp - strm->next_out;
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
    while(state->out_block + state->i < state->out_bp)
    {
        if (strm->avail_out == 0)
            return M_EXIT;

        *strm->next_out++ = state->out_block[state->i];
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
    {
        return AE_ERRNO;
    }

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
        state->in_blklen = 4 * strm->block_size;

        if (strm->flags & AE_DATA_MSB)
            state->get_sample = get_msb_32;
        else
            state->get_sample = get_lsb_32;
    }
    else if (strm->bit_per_sample > 8)
    {
        /* 16 bit settings */
        state->id_len = 4;
        state->in_blklen = 2 * strm->block_size;

        if (strm->flags & AE_DATA_MSB)
        {
            state->get_sample = get_msb_16;

            if (strm->block_size == 8)
                state->get_block = get_block_msb_16_bs_8;
            else
                state->get_block = get_block_msb_16_bs_16;
        }
        else
            state->get_sample = get_lsb_16;
    }
    else
    {
        /* 8 bit settings */
        state->in_blklen = strm->block_size;
        state->id_len = 3;

        state->get_sample = get_8;

        if (strm->block_size == 8)
            state->get_block = get_block_8_bs_8;
        else
            state->get_block = get_block_8_bs_16;
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

    state->in_block = (int64_t *)malloc(strm->block_size * sizeof(int64_t));
    if (state->in_block == NULL)
    {
        return AE_MEM_ERROR;
    }

    /* Largest possible block according to specs */
    state->out_blklen = (5 + 16 * 32) / 8 + 3;
    /* Output buffer */
    state->out_block = (uint8_t *)malloc(state->out_blklen);
    if (state->out_block == NULL)
    {
        return AE_MEM_ERROR;
    }

    strm->total_in = 0;
    strm->total_out = 0;

    state->out_bp = state->out_block;
    *state->out_bp = 0;
    state->bitp = 8;
    state->mode = m_get_block;

    return AE_OK;
}

int ae_encode(ae_streamp strm, int flush)
{
    /**
       Finite-state machine implementation of the adaptive entropy
       encoder.
    */

    encode_state *state;
    state = strm->state;
    state->flush = flush;

    while (state->mode(strm) == M_CONTINUE);

    if (state->out_direct)
    {
        m_flush_block(strm);
        *state->out_block = *state->out_bp;
        state->out_bp = state->out_block;
        state->out_direct = 0;
    }
    return AE_OK;
}

int ae_encode_end(ae_streamp strm)
{
    encode_state *state = strm->state;

    free(state->in_block);
    free(state->out_block);
    free(state);
    return AE_OK;
}
