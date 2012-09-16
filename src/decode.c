/* Adaptive Entropy Decoder            */
/* CCSDS 121.0-B-1 and CCSDS 120.0-G-2 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include "libaec.h"

#define MIN(a, b) (((a) < (b))? (a): (b))

#define SAFE (strm->avail_in >= state->in_blklen        \
              && strm->avail_out >= state->out_blklen)

#define ROS 5

typedef struct internal_state {
    int id;            /* option ID */
    int id_len;        /* bit length of code option identification key */
    int *id_table;     /* table maps IDs to states */
    void (*put_sample)(aec_streamp, int64_t);
    int ref_int;       /* reference sample is every ref_int samples */
    int64_t last_out;  /* previous output for post-processing */
    int64_t xmin;      /* minimum integer for post-processing */
    int64_t xmax;      /* maximum integer for post-processing */
    int mode;          /* current mode of FSM */
    int in_blklen;     /* length of uncompressed input block
                          should be the longest possible block */
    int out_blklen;    /* length of output block in bytes */
    int n, i;          /* counter for samples */
    int64_t *block;    /* block buffer for split-sample options */
    int se;            /* set if second extension option is selected */
    uint64_t acc;      /* accumulator for currently used bit sequence */
    int bitp;          /* bit pointer to the next unused bit in accumulator */
    int fs;            /* last fundamental sequence in accumulator */
    int ref;           /* 1 if current block has reference sample */
    int pp;            /* 1 if postprocessor has to be used */
    int byte_per_sample;
    size_t samples_out;
} decode_state;

/* decoding table for the second-extension option */
static const int second_extension[92][2] =
{
    {0, 0},
    {1, 1}, {1, 1},
    {2, 3}, {2, 3}, {2, 3},
    {3, 6}, {3, 6}, {3, 6}, {3, 6},
    {4, 10}, {4, 10}, {4, 10}, {4, 10}, {4, 10},
    {5, 15}, {5, 15}, {5, 15}, {5, 15}, {5, 15}, {5, 15},
    {6, 21}, {6, 21}, {6, 21}, {6, 21}, {6, 21}, {6, 21}, {6, 21},
    {7, 28}, {7, 28}, {7, 28}, {7, 28}, {7, 28}, {7, 28}, {7, 28}, {7, 28},
    {8, 36}, {8, 36}, {8, 36}, {8, 36}, {8, 36}, {8, 36}, {8, 36}, {8, 36}, {8, 36},
    {9, 45}, {9, 45}, {9, 45}, {9, 45}, {9, 45}, {9, 45}, {9, 45}, {9, 45}, {9, 45}, {9, 45},
    {10, 55}, {10, 55}, {10, 55}, {10, 55}, {10, 55}, {10, 55}, {10, 55}, {10, 55}, {10, 55}, {10, 55}, {10, 55},
    {11, 66}, {11, 66}, {11, 66}, {11, 66}, {11, 66}, {11, 66}, {11, 66}, {11, 66}, {11, 66}, {11, 66}, {11, 66}, {11, 66},
    {12, 78}, {12, 78}, {12, 78}, {12, 78}, {12, 78}, {12, 78}, {12, 78}, {12, 78}, {12, 78}, {12, 78}, {12, 78}, {12, 78}, {12, 78}
};

enum
{
    M_ID = 0,
    M_SPLIT,
    M_SPLIT_FS,
    M_SPLIT_OUTPUT,
    M_LOW_ENTROPY,
    M_LOW_ENTROPY_REF,
    M_ZERO_BLOCK,
    M_ZERO_OUTPUT,
    M_SE,
    M_SE_DECODE,
    M_UNCOMP,
    M_UNCOMP_COPY,
};

static void put_msb_32(aec_streamp strm, int64_t data)
{
    *strm->next_out++ = data >> 24;
    *strm->next_out++ = data >> 16;
    *strm->next_out++ = data >> 8;
    *strm->next_out++ = data;
    strm->avail_out -= 4;
    strm->total_out += 4;
}

static void put_msb_24(aec_streamp strm, int64_t data)
{
    *strm->next_out++ = data >> 16;
    *strm->next_out++ = data >> 8;
    *strm->next_out++ = data;
    strm->avail_out -= 3;
    strm->total_out += 3;
}

static void put_msb_16(aec_streamp strm, int64_t data)
{
    *strm->next_out++ = data >> 8;
    *strm->next_out++ = data;
    strm->avail_out -= 2;
    strm->total_out += 2;
}

static void put_lsb_32(aec_streamp strm, int64_t data)
{
    *strm->next_out++ = data;
    *strm->next_out++ = data >> 8;
    *strm->next_out++ = data >> 16;
    *strm->next_out++ = data >> 24;
    strm->avail_out -= 4;
    strm->total_out += 4;
}

static void put_lsb_24(aec_streamp strm, int64_t data)
{
    *strm->next_out++ = data;
    *strm->next_out++ = data >> 8;
    *strm->next_out++ = data >> 16;
    strm->avail_out -= 3;
    strm->total_out += 3;
}

static void put_lsb_16(aec_streamp strm, int64_t data)
{
    *strm->next_out++ = data;
    *strm->next_out++ = data >> 8;
    strm->avail_out -= 2;
    strm->total_out += 2;
}

static void put_8(aec_streamp strm, int64_t data)
{
    *strm->next_out++ = data;
    strm->avail_out--;
    strm->total_out++;
}

static inline void u_put(aec_streamp strm, int64_t sample)
{
    int64_t x, d, th, D, lower;
    decode_state *state = strm->state;

    if (state->pp && (state->samples_out % state->ref_int != 0))
    {
        d = sample;
        x = state->last_out;
        lower = x - state->xmin;
        th = MIN(lower, state->xmax - x);

        if (d <= 2 * th)
        {
            if (d & 1)
                D = - (d + 1) / 2;
            else
                D = d / 2;
        } else {
            if (th == lower)
                D = d - th;
            else
                D = th - d;
        }
        sample = x + D;
    }
    else
    {
        if (strm->flags & AEC_DATA_SIGNED)
        {
            int m = 64 - strm->bit_per_sample;
            /* Reference samples have to be sign extended */
            sample = (sample << m) >> m;
        }
    }
    state->last_out = sample;
    state->put_sample(strm, sample);
    state->samples_out++;
}

static inline int64_t u_get(aec_streamp strm, unsigned int n)
{
    /**
       Unsafe get n bit from input stream

       No checking whatsoever. Read bits are dumped.
     */

    decode_state *state;

    state = strm->state;
    while (state->bitp < n)
    {
        strm->avail_in--;
        strm->total_in++;
        state->acc = (state->acc << 8) | *strm->next_in++;
        state->bitp += 8;
    }
    state->bitp -= n;
    return (state->acc >> state->bitp) & ((1ULL << n) - 1);
}

static inline int64_t u_get_fs(aec_streamp strm)
{
    /**
       Interpret a Fundamental Sequence from the input buffer.

       Essentially counts the number of 0 bits until a
       1 is encountered. TODO: faster version.
     */

    int64_t fs = 0;

    while(u_get(strm, 1) == 0)
        fs++;

    return fs;
}

static inline void fast_split(aec_streamp strm)
{
    int i, k;
    decode_state *state;

    state = strm->state;
    k = state->id - 1;

    if (state->ref)
        u_put(strm, u_get(strm, strm->bit_per_sample));

    for (i = state->ref; i < strm->block_size; i++)
        state->block[i] = u_get_fs(strm) << k;

    for (i = state->ref; i < strm->block_size; i++)
    {
        state->block[i] += u_get(strm, k);
        u_put(strm, state->block[i]);
    }
}

static inline void fast_zero(aec_streamp strm)
{
    int i = strm->state->i;

    while (i--)
        u_put(strm, 0);
}

static inline void fast_se(aec_streamp strm)
{
    int i;
    int64_t gamma, beta, ms, delta1;

    i = strm->state->ref;

    while (i < strm->block_size)
    {
        gamma = u_get_fs(strm);
        beta = second_extension[gamma][0];
        ms = second_extension[gamma][1];
        delta1 = gamma - ms;

        if ((i & 1) == 0)
        {
            u_put(strm, beta - delta1);
            i++;
        }
        u_put(strm, delta1);
        i++;
    }
}

static inline void fast_uncomp(aec_streamp strm)
{
    int i;

    for (i = 0; i < strm->block_size; i++)
        u_put(strm, u_get(strm, strm->bit_per_sample));
}

int aec_decode_init(aec_streamp strm)
{
    int i, modi;
    decode_state *state;

    /* Some sanity checks */
    if (strm->bit_per_sample > 32 || strm->bit_per_sample == 0)
    {
        return AEC_CONF_ERROR;
    }

    /* Internal state for decoder */
    state = (decode_state *) malloc(sizeof(decode_state));
    if (state == NULL)
    {
        return AEC_MEM_ERROR;
    }
    strm->state = state;

    if (strm->bit_per_sample > 16)
    {
        state->id_len = 5;

        if (strm->bit_per_sample <= 24 && strm->flags & AEC_DATA_3BYTE)
        {
            state->byte_per_sample = 3;
            if (strm->flags & AEC_DATA_MSB)
                state->put_sample = put_msb_24;
            else
                state->put_sample = put_lsb_24;
        }
        else
        {
            state->byte_per_sample = 4;
            if (strm->flags & AEC_DATA_MSB)
                state->put_sample = put_msb_32;
            else
                state->put_sample = put_lsb_32;
        }
        state->out_blklen = strm->block_size
            * state->byte_per_sample;
    }
    else if (strm->bit_per_sample > 8)
    {
        state->byte_per_sample = 2;
        state->id_len = 4;
        state->out_blklen = strm->block_size * 2;
        if (strm->flags & AEC_DATA_MSB)
            state->put_sample = put_msb_16;
        else
            state->put_sample = put_lsb_16;
    }
    else
    {
        state->byte_per_sample = 1;
        state->id_len = 3;
        state->out_blklen = strm->block_size;
        state->put_sample = put_8;
    }

    if (strm->flags & AEC_DATA_SIGNED)
    {
        state->xmin = -(1ULL << (strm->bit_per_sample - 1));
        state->xmax = (1ULL << (strm->bit_per_sample - 1)) - 1;
    }
    else
    {
        state->xmin = 0;
        state->xmax = (1ULL << strm->bit_per_sample) - 1;
    }

    state->ref_int = strm->block_size * strm->rsi;
    state->in_blklen = (strm->block_size * strm->bit_per_sample
                        + state->id_len) / 8 + 1;

    modi = 1UL << state->id_len;
    state->id_table = (int *)malloc(modi * sizeof(int));
    if (state->id_table == NULL)
    {
        return AEC_MEM_ERROR;
    }
    state->id_table[0] = M_LOW_ENTROPY;
    for (i = 1; i < modi - 1; i++)
    {
        state->id_table[i] = M_SPLIT;
    }
    state->id_table[modi - 1] = M_UNCOMP;

    state->block = (int64_t *)malloc(strm->block_size * sizeof(int64_t));
    if (state->block == NULL)
    {
        return AEC_MEM_ERROR;
    }
    strm->total_in = 0;
    strm->total_out = 0;

    state->samples_out = 0;
    state->bitp = 0;
    state->fs = 0;
    state->pp = strm->flags & AEC_DATA_PREPROCESS;
    state->mode = M_ID;
    return AEC_OK;
}

int aec_decode_end(aec_streamp strm)
{
    decode_state *state;

    state = strm->state;
    free(state->block);
    free(state->id_table);
    free(state);
    return AEC_OK;
}

#define ASK(n)                                           \
    do {                                                 \
        while (state->bitp < (unsigned)(n))              \
        {                                                \
            if (strm->avail_in == 0) goto req_buffer;    \
            strm->avail_in--;                            \
            strm->total_in++;                            \
            state->acc <<= 8;                            \
            state->acc |= *strm->next_in++;              \
            state->bitp += 8;                            \
        }                                                \
    } while (0)

#define GET(n)                                                  \
    ((state->acc >> (state->bitp - (n))) & ((1ULL << (n)) - 1))

#define DROP(n) state->bitp -= (unsigned)(n)

#define ASKFS()                                                 \
    do {                                                        \
        ASK(1);                                                 \
        while ((state->acc & (1ULL << (state->bitp - 1))) == 0) \
        {                                                       \
            if (state->bitp == 1)                               \
            {                                                   \
                if (strm->avail_in == 0) goto req_buffer;       \
                strm->avail_in--;                               \
                strm->total_in++;                               \
                state->acc <<= 8;                               \
                state->acc |= *strm->next_in++;                 \
                state->bitp += 8;                               \
            }                                                   \
            state->fs++;                                        \
            state->bitp--;                                      \
        }                                                       \
    } while (0)

#define GETFS() state->fs

#define DROPFS()                                \
    do {                                        \
        state->fs = 0;                          \
        /* Needs to be here for                 \
           ASK/GET/PUT/DROP interleaving. */    \
        state->bitp--;                          \
    } while (0)

#define PUT(sample)                                \
    do {                                           \
        if (strm->avail_out == 0) goto req_buffer; \
        u_put(strm, (sample));                     \
    } while (0)

#define COPYSAMPLE()                    \
    do {                                \
        ASK(strm->bit_per_sample);      \
        PUT(GET(strm->bit_per_sample)); \
        DROP(strm->bit_per_sample);     \
    } while (0)


int aec_decode(aec_streamp strm, int flush)
{
    /**
       Finite-state machine implementation of the adaptive entropy
       decoder.

       Can work with one byte input und one sample output buffers. If
       enough buffer space is available, then faster implementations
       of the states are called. Inspired by zlib.
    */

    int zero_blocks;
    int64_t gamma, beta, ms, delta1;
    int k;
    decode_state *state;

    state = strm->state;

    for (;;)
    {
        switch(state->mode)
        {
        case M_ID:
            if (state->pp
                && (state->samples_out / strm->block_size) % strm->rsi == 0)
                state->ref = 1;
            else
                state->ref = 0;

            ASK(state->id_len);
            state->id = GET(state->id_len);
            DROP(state->id_len);
            state->mode = state->id_table[state->id];
            break;

        case M_SPLIT:
            if (SAFE)
            {
                fast_split(strm);
                state->mode = M_ID;
                break;
            }

            if (state->ref)
            {
                COPYSAMPLE();
                state->n = strm->block_size - 1;
            }
            else
            {
                state->n = strm->block_size;
            }

            state->i = state->n - 1;
            state->mode = M_SPLIT_FS;

        case M_SPLIT_FS:
            do
            {
                ASKFS();
                state->block[state->i] = GETFS();
                DROPFS();
            }
            while(state->i--);

            state->i = state->n - 1;
            state->mode = M_SPLIT_OUTPUT;

        case M_SPLIT_OUTPUT:
            k = state->id - 1;
            do
            {
                ASK(k);
                PUT((state->block[state->i] << k) + GET(k));
                DROP(k);
            }
            while(state->i--);

            state->mode = M_ID;
            break;

        case M_LOW_ENTROPY:
            ASK(1);
            state->id = GET(1);
            DROP(1);
            state->mode = M_LOW_ENTROPY_REF;

        case M_LOW_ENTROPY_REF:
            if (state->ref)
                COPYSAMPLE();

            if(state->id == 1)
            {
                state->mode = M_SE;
                break;
            }

            state->mode = M_ZERO_BLOCK;

        case M_ZERO_BLOCK:
            ASKFS();
            zero_blocks = GETFS() + 1;
            DROPFS();

            if (zero_blocks == ROS)
            {
                zero_blocks =  64 - (
                    (state->samples_out / strm->block_size)
                    % strm->rsi % 64);
            }
            else if (zero_blocks > ROS)
            {
                zero_blocks--;
            }

            if (state->ref)
                state->i = zero_blocks * strm->block_size - 1;
            else
                state->i = zero_blocks * strm->block_size;

            if (strm->avail_out >= state->i * state->byte_per_sample)
            {
                fast_zero(strm);
                state->mode = M_ID;
                break;
            }

            state->mode = M_ZERO_OUTPUT;

        case M_ZERO_OUTPUT:
            do
                PUT(0);
            while(--state->i);

            state->mode = M_ID;
            break;

        case M_SE:
            if (SAFE)
            {
                fast_se(strm);
                state->mode = M_ID;
                break;
            }

            state->mode = M_SE_DECODE;
            state->i = state->ref;

        case M_SE_DECODE:
            while(state->i < strm->block_size)
            {
                ASKFS();
                gamma = GETFS();
                beta = second_extension[gamma][0];
                ms = second_extension[gamma][1];
                delta1 = gamma - ms;

                if ((state->i & 1) == 0)
                {
                    PUT(beta - delta1);
                    state->i++;
                }

                PUT(delta1);
                state->i++;
                DROPFS();
            }

            state->mode = M_ID;
            break;

        case M_UNCOMP:
            if (SAFE)
            {
                fast_uncomp(strm);
                state->mode = M_ID;
                break;
            }

            state->i = strm->block_size;
            state->mode = M_UNCOMP_COPY;

        case M_UNCOMP_COPY:
            do
                COPYSAMPLE();
            while(--state->i);

            state->mode = M_ID;
            break;

        default:
            return AEC_STREAM_ERROR;
        }
    }

req_buffer:
    return AEC_OK;
}
