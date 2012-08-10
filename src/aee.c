/* Adaptive Entropy Encoder            */
/* CCSDS 121.0-B-1 and CCSDS 120.0-G-2 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
    int id_len;             /* bit length of code option identification key */
    int64_t last_in;        /* previous input for preprocessing */
    int64_t (*get_sample)(ae_streamp);
    int64_t xmin;           /* minimum integer for preprocessing */
    int64_t xmax;           /* maximum integer for preprocessing */
    int mode;               /* current mode of FSM */
    int i;                  /* counter for samples */
    int64_t *block_in;      /* input block buffer */
    uint8_t *block_out;     /* output block buffer */
    uint8_t *bp_out;        /* pointer to current output */
    size_t total_blocks;
    int bitp;               /* bit pointer to the next unused bit in accumulator */
    int block_deferred;     /* there is a block in the input buffer
                               but we first have to emit a zero block */
    int ref;                /* length of reference sample in current block
                               i.e. 0 or 1 depending on whether the block has
                               a reference sample or not */
    int zero_ref;           /* current zero block has a reference sample */
    int64_t zero_ref_sample;/* reference sample of zero block */
    int zero_blocks;        /* number of contiguous zero blocks */
#ifdef PROFILE
    int *prof;
#endif
} encode_state;

static int64_t get_lsb_32(ae_streamp strm)
{
    int64_t data;

    data = (strm->next_in[3] << 24)
        | (strm->next_in[2] << 16)
        | (strm->next_in[1] << 8)
        | strm->next_in[0];

    strm->next_in += 4;
    strm->total_in += 4;
    strm->avail_in -= 4;
    return data;
}

static int64_t get_lsb_16(ae_streamp strm)
{
    int64_t data;

    data = (strm->next_in[1] << 8) | strm->next_in[0];

    strm->next_in += 2;
    strm->total_in += 2;
    strm->avail_in -= 2;
    return data;
}

static int64_t get_msb_32(ae_streamp strm)
{
    int64_t data;

    data = (strm->next_in[0] << 24)
        | (strm->next_in[1] << 16)
        | (strm->next_in[2] << 8)
        | strm->next_in[3];

    strm->next_in += 4;
    strm->total_in += 4;
    strm->avail_in -= 4;
    return data;
}

static int64_t get_msb_16(ae_streamp strm)
{
    int64_t data;

    data = (strm->next_in[0] << 8) | strm->next_in[1];

    strm->next_in += 2;
    strm->total_in += 2;
    strm->avail_in -= 2;
    return data;
}

static int64_t get_8(ae_streamp strm)
{
    strm->avail_in--;
    strm->total_in++;
    return *strm->next_in++;
}

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

    if (strm->bit_per_sample > 16)
    {
        state->id_len = 5;
        if (strm->flags & AE_DATA_MSB)
            state->get_sample = get_msb_32;
        else
            state->get_sample = get_lsb_32;
    }
    else if (strm->bit_per_sample > 8)
    {
        state->id_len = 4;
        if (strm->flags & AE_DATA_MSB)
            state->get_sample = get_msb_16;
        else
            state->get_sample = get_lsb_16;
    }
    else
    {
        state->id_len = 3;
        state->get_sample = get_8;
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

#ifdef PROFILE
    state->prof = (int *)malloc((strm->bit_per_sample + 2) * sizeof(int));
    if (state->prof == NULL)
    {
        return AE_MEM_ERROR;
    }
    memset(state->prof, 0, (strm->bit_per_sample + 2) * sizeof(int));
#endif

    state->block_in = (int64_t *)malloc(strm->block_size * sizeof(int64_t));
    if (state->block_in == NULL)
    {
        return AE_MEM_ERROR;
    }

    blklen = (strm->block_size * strm->bit_per_sample
              + state->id_len) / 8 + 2;

    state->block_out = (uint8_t *)malloc(blklen);
    if (state->block_out == NULL)
    {
        return AE_MEM_ERROR;
    }
    state->bp_out = state->block_out;
    state->bitp = 8;

    strm->total_in = 0;
    strm->total_out = 0;

    state->mode = M_NEW_BLOCK;

    state->total_blocks = 0;
    state->block_deferred = 0;
    state->zero_blocks = 0;
    state->zero_ref = 0;
    state->ref = 0;

    return AE_OK;
}

static inline void emit(encode_state *state, int64_t data, int bits)
{
    while(bits)
    {
        data &= ((1ULL << bits) - 1);
        if (bits <= state->bitp)
        {
            state->bitp -= bits;
            *state->bp_out += data << state->bitp;
            bits = 0;
        }
        else
        {
            bits -= state->bitp;
            *state->bp_out += data >> bits;
            *++state->bp_out = 0;
            state->bitp = 8;
        }
    }
}

static inline void emitfs(encode_state *state, int fs)
{
    emit(state, 1, fs + 1);
}

#ifdef PROFILE
static inline void profile_print(ae_streamp strm)
{
    int i, total;
    encode_state *state;

    state = strm->state;
    fprintf(stderr, "Blocks encoded by each coding option\n");
    fprintf(stderr, "Zero blocks:  %i\n", state->prof[0]);
    total = state->prof[0];
    fprintf(stderr, "Second Ext.:  %i\n", state->prof[strm->bit_per_sample+1]);
    total += state->prof[strm->bit_per_sample+1];
    fprintf(stderr, "FS:           %i\n", state->prof[1]);
    total += state->prof[1];
    for (i = 2; i < strm->bit_per_sample - 1; i++)
    {
        fprintf(stderr, "k = %02i:       %i\n", i-1, state->prof[i]);
        total += state->prof[i];
    }
    fprintf(stderr, "Uncompressed: %i\n", state->prof[strm->bit_per_sample]);
    total += state->prof[strm->bit_per_sample];
    fprintf(stderr, "Total blocks: %i\n", total);
}
#endif

int ae_encode(ae_streamp strm, int flush)
{
    /**
       Finite-state machine implementation of the adaptive entropy
       encoder.
    */

    int i, j, k, zb, this_bs;
    int64_t split_len;
    int64_t split_len_min, se_len, fs_len;
    int64_t d;
    int64_t theta, Delta;
    size_t avail_out, total_out;

    encode_state *state;

    state = strm->state;
    total_out = strm->total_out;
    avail_out = strm->avail_out;

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
                            /* pad block with last sample if we have
                               a partial block */
                            state->block_in[state->i] = state->block_in[state->i - 1];
                        }
                        else
                        {
                            /* Pad last output byte with 1 bits
                               if user wants to flush, i.e. we got
                               all input there is.
                            */
                            emit(state, 0xff, state->bitp);
                            *strm->next_out++ = *state->bp_out;
                            avail_out--;
                            total_out++;
#ifdef PROFILE
                            profile_print(strm);
#endif
                            goto req_buffer;
                        }
                    }
                    else
                    {
                        goto req_buffer;
                    }
                }
                else
                {
                    state->block_in[state->i] = state->get_sample(strm);
                }
            }
            while (++state->i < strm->block_size);

            state->total_blocks++;

            /* preprocess block if needed */
            if (strm->flags & AE_DATA_PREPROCESS)
            {
                /* If this is the first block in a segment
                   then we need to insert a reference sample.
                */
                if(state->total_blocks % strm->segment_size == 1)
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
                    Delta = state->block_in[i] - state->last_in;
                    state->last_in = state->block_in[i];
                    if (0 <= Delta && Delta <= theta)
                    {
                        state->block_in[i] = 2 * Delta;
                    }
                    else if (-theta <= Delta && Delta < 0)
                    {
                        d = Delta < 0 ? -(uint64_t)Delta : Delta;
                        state->block_in[i] = 2 * d - 1;
                    }
                    else
                    {
                        state->block_in[i] = theta +
                            (Delta < 0 ? -(uint64_t)Delta : Delta);
                    }
                }
            }
            state->mode = M_CHECK_ZERO_BLOCK;

        case M_CHECK_ZERO_BLOCK:
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

                if (state->total_blocks % strm->segment_size == 0)
                {
                    if (state->zero_blocks > ROS)
                        state->zero_blocks = ROS;
#ifdef PROFILE
                    state->prof[0] += state->zero_blocks;
#endif
                    state->mode = M_ENCODE_ZERO;
                    break;
                }
                state->mode = M_NEW_BLOCK;
                break;
            }
            else if (state->zero_blocks)
            {
#ifdef PROFILE
                state->prof[0] += state->zero_blocks;
#endif
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
            /* If zero block isn't an option then count length of
               sample splitting options */

            /* Baseline is the size of an uncompressed block */
            split_len_min = (strm->block_size - state->ref) * strm->bit_per_sample;
            k = strm->bit_per_sample;

            /* Length of this block minus reference sample if present */
            this_bs = strm->block_size - state->ref;

            /* Add FS encoded to unencoded parts */
            for (j = 0; j < strm->bit_per_sample - 2; j++)
            {
#ifdef UNROLL_BLOCK_8
                fs_len = (state->block_in[1] >> j)
                    + (state->block_in[2] >> j)
                    + (state->block_in[3] >> j)
                    + (state->block_in[4] >> j)
                    + (state->block_in[5] >> j)
                    + (state->block_in[6] >> j)
                    + (state->block_in[7] >> j);
                if (state->ref == 0)
                    fs_len += (state->block_in[0] >> j);
#else
                fs_len = 0;
                for (i = state->ref; i < strm->block_size; i++)
                    fs_len += state->block_in[i] >> j;
#endif
                split_len = fs_len + this_bs * (j + 1);
                if (split_len < split_len_min)
                {
                    split_len_min = split_len;
                    k = j;

                    if (fs_len < this_bs)
                    {
                        /* Next can't get better because what we lose
                           by additional uncompressed bits isn't
                           compensated by a smaller FS part. */
                        break;
                    }
                }
                else
                    break;
            }

            /* Count bits for 2nd extension */
            se_len = 1;
            for (i = 0; i < strm->block_size && split_len_min > se_len; i+= 2)
            {
                d = state->block_in[i] + state->block_in[i + 1];
                /* we have to worry about overflow here */
                if (d > split_len_min)
                    se_len = d;
                else
                    se_len += d * (d + 1) / 2 + state->block_in[i + 1];
            }

            /* Decide which option to use */
            if (split_len_min <= se_len)
            {
                if (k == strm->bit_per_sample)
                {
#ifdef PROFILE
                    state->prof[k]++;
#endif
                    state->mode = M_ENCODE_UNCOMP;
                    break;
                }
                else
                {
#ifdef PROFILE
                    state->prof[k + 1]++;
#endif
                    state->mode = M_ENCODE_SPLIT;
                }
            }
            else
            {
#ifdef PROFILE
                state->prof[strm->bit_per_sample + 1]++;
#endif
                state->mode = M_ENCODE_SE;
                break;
            }

            emit(state, k + 1, state->id_len);
            if (state->ref)
                emit(state, state->block_in[0], strm->bit_per_sample);

            for (i = state->ref; i < strm->block_size; i++)
                emitfs(state, state->block_in[i] >> k);

            for (i = state->ref; i < strm->block_size; i++)
                emit(state, state->block_in[i], k);

            state->mode = M_FLUSH_BLOCK;

        case M_FLUSH_BLOCK:
            if (strm->avail_in == 0 && flush == AE_FLUSH)
            {
                /* pad last byte with 1 bits */
                emit(state, 0xff, state->bitp);
            }
            state->i = 0;
            state->mode = M_FLUSH_BLOCK_LOOP;

        case M_FLUSH_BLOCK_LOOP:
            while(state->block_out + state->i < state->bp_out)
            {
                if (avail_out == 0)
                {
#ifdef PROFILE
                    profile_print(strm);
#endif
                    goto req_buffer;
                }

                *strm->next_out++ = state->block_out[state->i];
                avail_out--;
                total_out++;
                state->i++;
            }
            state->mode = M_NEW_BLOCK;
            break;

        case M_ENCODE_UNCOMP:
            emit(state, 0x1f, state->id_len);
            for (i = 0; i < strm->block_size; i++)
                emit(state, state->block_in[i], strm->bit_per_sample);

            state->mode = M_FLUSH_BLOCK;
            break;

        case M_ENCODE_SE:
            emit(state, 1, state->id_len + 1);
            if (state->ref)
                emit(state, state->block_in[0], strm->bit_per_sample);

            for (i = 0; i < strm->block_size; i+= 2)
            {
                d = state->block_in[i] + state->block_in[i + 1];
                emitfs(state, d * (d + 1) / 2 + state->block_in[i + 1]);
            }

            state->mode = M_FLUSH_BLOCK;
            break;

        case M_ENCODE_ZERO:
            emit(state, 0, state->id_len + 1);
            if (state->zero_ref)
            {
                emit(state, state->zero_ref_sample, strm->bit_per_sample);
            }
            emitfs(state, state->zero_blocks - 1);
            state->zero_blocks = 0;
            state->mode = M_FLUSH_BLOCK;
            break;

        default:
            return AE_STREAM_ERROR;
        }
    }

req_buffer:
    strm->total_out = total_out;
    strm->avail_out = avail_out;
    if (strm->avail_in == 0 && avail_out && flush == AE_FLUSH)
    {
#ifdef PROFILE
        free(state->prof);
#endif
        free(state->block_in);
        free(state->block_out);
        free(strm->state);
    }
    return AE_OK;
}
