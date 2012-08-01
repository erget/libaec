/* Adaptive Entropy Encoder            */
/* CCSDS 121.0-B-1 and CCSDS 120.0-G-2 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include "libae.h"

#define ROS 5

#define MIN(a, b) (((a) < (b))? (a): (b))

enum
{
    M_NEW_BLOCK,
    M_GET_BLOCK,
    M_CHECK_ZERO_BLOCK,
    M_SELECT_CODE_OPTION,
    M_ENCODE_SPLIT,
    M_FLUSH_BLOCK,
    M_FLUSH_BLOCK_LOOP,
    M_ENCODE_UNCOMP,
    M_ENCODE_SE,
    M_ENCODE_ZERO,
};

typedef struct internal_state {
    const uint32_t *next_in;
    uint8_t *next_out;
    uint32_t id_len;        /* bit length of code option identification key */
    uint32_t last_in;       /* previous input for preprocessing */
    int64_t xmin;           /* minimum integer for preprocessing */
    int64_t xmax;           /* maximum integer for preprocessing */
    int mode;               /* current mode of FSM */
    size_t i;               /* counter for samples */
    uint32_t *block_in;     /* input block buffer */
    uint8_t *block_out;     /* output block buffer */
    uint8_t *bp_out;        /* pointer to current output */
    uint8_t bitp;           /* bit pointer to the next unused bit in accumulator */
    uint8_t block_deferred; /* there is a block in the input buffer
                               but we first have to emit a zero block */
    uint8_t ref;            /* current buffer has a reference sample */
    uint8_t zero_ref;       /* current zero block has a reference sample */
    uint32_t zero_ref_sample; /* reference sample of zero block */
    size_t zero_blocks;     /* number of contiguous zero blocks */
} encode_state;

int ae_encode_init(ae_streamp strm)
{
    int blklen;
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
    strm->state = state;

    if (16 < strm->bit_per_sample)
        state->id_len = 5;
    else if (8 < strm->bit_per_sample)
        state->id_len = 4;
    else
        state->id_len = 3;

    state->block_in = (uint32_t *)malloc(strm->block_size * sizeof(uint32_t));
    if (state->block_in == NULL)
    {
        return AE_MEM_ERROR;
    }

    blklen = (strm->block_size * strm->bit_per_sample
              + state->id_len) / 8 + 1;

    state->block_out = (uint8_t *)malloc(blklen);
    if (state->block_out == NULL)
    {
        return AE_MEM_ERROR;
    }
    state->bp_out = state->block_out;
    state->bitp = 8;

    strm->total_in = 0;
    strm->total_out = 0;
    state->xmin = 0;
    state->xmax = (1ULL << strm->bit_per_sample) - 1;

    state->mode = M_NEW_BLOCK;

    state->block_deferred = 0;
    state->zero_ref = 0;
    state->ref = 0;

    return AE_OK;
}

#define EMIT(d, n)                                              \
    do                                                          \
    {                                                           \
        int bits = (n);                                         \
        uint32_t data = (d);                                    \
        while(bits)                                             \
        {                                                       \
            data &= ((1UL << bits) - 1);                        \
            if (bits <= state->bitp)                            \
            {                                                   \
                data <<= state->bitp - bits;                    \
                *state->bp_out += data;                         \
                state->bitp -= bits;                            \
                bits = 0;                                       \
            }                                                   \
            else                                                \
            {                                                   \
                *state->bp_out += data >> (bits - state->bitp); \
                bits -= state->bitp;                            \
                *++state->bp_out = 0;                           \
                state->bitp = 8;                                \
            }                                                   \
        }                                                       \
    }                                                           \
    while (0)


#define EMITFS(d)                                               \
    do                                                          \
    {                                                           \
        EMIT(0, d);                                             \
        EMIT(1, 1);                                             \
    }                                                           \
    while (0)

int ae_encode(ae_streamp strm, int flush)
{
    /**
       Finite-state machine implementation of the adaptive entropy
       encoder.
    */

    int i, j, zb;
    int k_len[strm->bit_per_sample - 2];
    int k, k_min, se_len, blk_head;
    uint32_t d;
    int64_t theta, Delta;

    encode_state *state;

    state = strm->state;
    state->next_in = strm->next_in;
    state->next_out = strm->next_out;

    for (;;)
    {
        switch(state->mode)
        {
        case M_NEW_BLOCK:
            if (state->zero_blocks == 0)
            {
                /* copy leftover from last block */
                *state->block_out = *state->bp_out;
                state->bp_out = state->block_out;
            }

            if(state->block_deferred)
            {
                state->block_deferred = 0;
                state->mode = M_SELECT_CODE_OPTION;
                break;
            }

            state->i = 0;
            state->mode = M_GET_BLOCK;

        case M_GET_BLOCK:
            do
            {
                if (strm->avail_in == 0)
                {
                    if (flush == AE_FLUSH)
                    {
                        if (state->i > 0)
                        {
                            /* pad block with zeros if we have
                               a partial block */
                            state->block_in[state->i] = 0;
                        }
                        else
                        {
                            /* Pad last output byte with 1 bits
                               if user wants to flush, i.e. we got
                               all input there is.
                            */
                            EMIT(0xff, state->bitp);
                            *state->next_out++ = *state->bp_out;
                            strm->avail_out--;
                            strm->total_out++;
                        }
                    }
                    goto req_buffer;
                }
                else
                {
                    state->block_in[state->i] = *state->next_in++;
                    strm->avail_in--;
                    strm->total_in++;
                }
            }
            while (++state->i < strm->block_size);

            /* preprocess block if needed */
            if (strm->pp)
            {
                /* If this is the first block in a segment
                   then we need to insert a reference sample.
                */
                if((strm->total_in / strm->block_size) % strm->segment_size == 1)
                {
                    state->ref = 1;
                    state->last_in = state->block_in[0];
                }
                else
                {
                    state->ref = 0;
                }

                for (i = state->ref; i < strm->block_size; i++)
                {
                    theta = MIN(state->last_in - state->xmin,
                                state->xmax - state->last_in);
                    Delta = (long long)state->block_in[i] - (long long)state->last_in;

                    if (0 <= Delta && Delta <= theta)
                        d = 2 * Delta;
                    else if (-theta <= Delta && Delta < 0)
                        d = 2 * llabs(Delta) - 1;
                    else
                        d = theta + llabs(Delta);

                    state->last_in = state->block_in[i];
                    state->block_in[i] = d;
                }
            }
            state->mode = M_CHECK_ZERO_BLOCK;

        case M_CHECK_ZERO_BLOCK:
            /* Check zero block */
            zb = 1;
            for (i = state->ref; i < strm->block_size && zb; i++)
                if (state->block_in[i] != 0) zb = 0;

            if (zb)
            {
                /* remember ref on first zero block */
                if (state->zero_blocks == 0)
                {
                    state->zero_ref = state->ref;
                    state->zero_ref_sample = state->block_in[0];
                }

                state->zero_blocks++;

                if ((strm->total_in / strm->block_size)
                    % strm->segment_size == 0)
                {
                    if (state->zero_blocks > ROS)
                        state->zero_blocks = ROS;
                    state->mode = M_ENCODE_ZERO;
                    break;
                }
                state->mode = M_NEW_BLOCK;
                break;
            }
            else if (state->zero_blocks)
            {
                state->mode = M_ENCODE_ZERO;
                /* The current block isn't zero but we have to
                   emit a previous zero block first. The
                   current block has to be handled later.
                */
                state->block_deferred = 1;
                break;
            }

            state->mode = M_SELECT_CODE_OPTION;

        case M_SELECT_CODE_OPTION:
            /* Encoded block always starts with ID and possibly
               a reference sample. */
            blk_head = state->id_len;
            if (state->ref)
                blk_head += strm->bit_per_sample;

            for (j = 0; j < strm->bit_per_sample - 2; j++)
                k_len[j] = blk_head;

            /* Count bits for sample splitting options */
            for (i = state->ref; i < strm->block_size; i++)
                for (j = 0; j < strm->bit_per_sample - 2; j++)
                    k_len[j] += (state->block_in[i] >> j) + 1 + j;

            /* Baseline is the size of an uncompressed block */
            k_min = state->id_len + strm->block_size * strm->bit_per_sample;
            k = strm->bit_per_sample;

            /* See if splitting option is better */
            for (j = 0; j < strm->bit_per_sample - 2; j++)
            {
                if (k_len[j] < k_min)
                {
                    k = j;
                    k_min = k_len[j];
                }
            }

            /* Count bits for 2nd extension */
            se_len = blk_head + 1;

            for (i = 0; i < strm->block_size && k_min > se_len; i+= 2)
            {
                d = state->block_in[i] + state->block_in[i + 1];
                se_len += d * (d + 1) / 2 + state->block_in[i + 1];
            }

            /* Decide which option to use */
            if (k_min <= se_len)
            {
                if (k == strm->bit_per_sample)
                {
                    state->mode = M_ENCODE_UNCOMP;
                    break;
                }
                else
                {
                    state->mode = M_ENCODE_SPLIT;
                }
            }
            else
            {
                state->mode = M_ENCODE_SE;
                break;
            }

        case M_ENCODE_SPLIT:
            EMIT(k + 1, state->id_len);
            if (state->ref)
                EMIT(state->block_in[0], strm->bit_per_sample);

            for (i = state->ref; i < strm->block_size; i++)
                EMITFS(state->block_in[i] >> k);

            for (i = state->ref; i < strm->block_size; i++)
                EMIT(state->block_in[i], k);

            state->mode = M_FLUSH_BLOCK;

        case M_FLUSH_BLOCK:
            if (strm->avail_in == 0 && flush == AE_FLUSH)
            {
                /* pad last byte with 1 bits */
                EMIT(0xff, state->bitp);
            }
            state->i = 0;
            state->mode = M_FLUSH_BLOCK_LOOP;

        case M_FLUSH_BLOCK_LOOP:
            while(state->block_out + state->i < state->bp_out)
            {
                if (strm->avail_out == 0)
                    goto req_buffer;

                *state->next_out++ = state->block_out[state->i];
                strm->avail_out--;
                strm->total_out++;
                state->i++;
            }
            state->mode = M_NEW_BLOCK;
            break;

        case M_ENCODE_UNCOMP:
            EMIT(0x1f, state->id_len);
            for (i = 0; i < strm->block_size; i++)
                EMIT(state->block_in[i], strm->bit_per_sample);

            state->mode = M_FLUSH_BLOCK;
            break;

        case M_ENCODE_SE:
            EMIT(1, state->id_len + 1);
            if (state->ref)
                EMIT(state->block_in[0], strm->bit_per_sample);

            for (i = 0; i < strm->block_size; i+= 2)
            {
                d = state->block_in[i] + state->block_in[i + 1];
                EMITFS(d * (d + 1) / 2 + state->block_in[i + 1]);
            }

            state->mode = M_FLUSH_BLOCK;
            break;

        case M_ENCODE_ZERO:
            EMIT(0, state->id_len + 1);
            if (state->zero_ref)
            {
                EMIT(state->zero_ref_sample, strm->bit_per_sample);
            }
            EMITFS(state->zero_blocks - 1);
            state->zero_blocks = 0;
            state->mode = M_FLUSH_BLOCK;
            break;

        default:
            return AE_STREAM_ERROR;
        }
    }

req_buffer:
    strm->next_in = state->next_in;
    strm->next_out = state->next_out;
    return AE_OK;
}
