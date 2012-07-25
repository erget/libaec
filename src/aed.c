/* Adaptive Entropy Decoder            */
/* CCSDS 121.0-B-1 and CCSDS 120.0-G-2 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include "aecd.h"

#define REFBLOCK (strm->pp && (strm->total_out / strm->block_size)  \
                    % strm->segment_size == 0)
#define SAFE (strm->avail_in >= state->in_blklen        \
              && strm->avail_out >= strm->block_size)

#define ROS 5

typedef struct internal_state {
    int id;            /* option ID */
    uint32_t id_len;   /* bit length of code option identification key */
    int *id_table;     /* table maps IDs to states */
    size_t ref_int;    /* reference sample is every ref_int samples */
    uint32_t last_out; /* previous output for post-processing */
    int64_t xmin;      /* minimum integer for post-processing */
    int64_t xmax;      /* maximum integer for post-processing */
    int mode;          /* current mode of FSM */
    size_t in_blklen;  /* length of uncompressed input block
                          should be the longest possible block */
    size_t n, i;       /* counter for samples */
    uint32_t *block;   /* block buffer for split-sample options */
    int se;            /* set if second extension option is selected */
    uint64_t acc;      /* accumulator for currently used bit sequence */
    uint8_t bitp;      /* bit pointer to the next unused bit in accumulator */
    uint32_t fs;       /* last fundamental sequence in accumulator */
} decode_state;

/* decoding table for the second-extension option */
static const uint32_t second_extension[36][2] = {
    {0, 0},
    {1, 1}, {1, 1},
    {2, 3}, {2, 3}, {2, 3},
    {3, 6}, {3, 6}, {3, 6}, {3, 6},
    {4, 10}, {4, 10}, {4, 10}, {4, 10}, {4, 10},
    {5, 15}, {5, 15}, {5, 15}, {5, 15}, {5, 15}, {5, 15},
    {6, 21}, {6, 21}, {6, 21}, {6, 21}, {6, 21}, {6, 21}, {6, 21},
    {7, 28}, {7, 28}, {7, 28}, {7, 28}, {7, 28}, {7, 28}, {7, 28}, {7, 28}
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

static inline void u_put(ae_streamp strm, uint32_t sample)
{
    int64_t x, d, th, D;
    decode_state *state;

    state = strm->state;
    if (strm->pp && (strm->total_out % state->ref_int != 0))
    {
        d = sample;
        x = state->last_out;

        if ((x - state->xmin) < (state->xmax - x))
        {
            th = x - state->xmin;
        }
        else
        {
            th = state->xmax - x;
        }

        if (d <= 2*th)
        {
            if (d & 1)
                D = - (d + 1) / 2;
            else
                D = d / 2;
        } else {
            if (th == x)
                D = d - th;
            else
                D = th - d;
        }
        sample = x + D;
    }
    *strm->next_out++ = state->last_out = sample;
    strm->avail_out--;
    strm->total_out++;
}

static inline uint32_t u_get(ae_streamp strm, unsigned int n)
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
        state->acc = (state->acc << 8) + *strm->next_in++;
        state->bitp += 8;
    }
    state->bitp -= n;
    return (state->acc >> state->bitp) & ((1ULL << n) - 1);
}

static inline uint32_t u_get_fs(ae_streamp strm)
{
    /**
       Interpret a Fundamental Sequence from the input buffer.

       Essentially counts the number of 0 bits until a
       1 is encountered. TODO: faster version.
     */

    uint32_t fs = 0;

    while(u_get(strm, 1) == 0)
        fs++;

    return fs;
}

static inline void fast_split(ae_streamp strm)
{
    int i, start, k;
    decode_state *state;

    state = strm->state;
    start = 0;
    k = state->id - 1;

    if (REFBLOCK)
    {
        start = 1;
        u_put(strm, u_get(strm, strm->bit_per_sample));
    }

    for (i = start; i < strm->block_size; i++)
    {
        state->block[i] = u_get_fs(strm) << k;
    }

    for (i = start; i < strm->block_size; i++)
    {
        state->block[i] += u_get(strm, k);
        u_put(strm, state->block[i]);
    }
}

static inline void fast_zero(ae_streamp strm)
{
    int i = strm->state->i;

    while (i--)
        u_put(strm, 0);
}

static inline void fast_se(ae_streamp strm)
{
    int i;
    uint32_t gamma, beta, ms, delta1;

    i = REFBLOCK? 1: 0;

    while (i < strm->bit_per_sample)
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

static inline void fast_uncomp(ae_streamp strm)
{
    int i;

    for (i = 0; i < strm->block_size; i++)
        u_put(strm, u_get(strm, strm->bit_per_sample));
}

int ae_decode_init(ae_streamp strm)
{
    int i, modi;
    decode_state *state;

    /* Some sanity checks */
    if (strm->bit_per_sample > 32 || strm->bit_per_sample == 0)
    {
        return AE_ERRNO;
    }

    /* Internal state for decoder */
    state = (decode_state *) malloc(sizeof(decode_state));
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

    state->ref_int = strm->block_size * strm->segment_size;
    state->in_blklen = (strm->block_size * strm->bit_per_sample
                        + state->id_len) / 8 + 1;

    modi = 1UL << state->id_len;
    state->id_table = (int *)malloc(modi * sizeof(int));
    if (state->id_table == NULL)
    {
        return AE_MEM_ERROR;
    }
    state->id_table[0] = M_LOW_ENTROPY;
    for (i = 1; i < modi - 1; i++)
    {
        state->id_table[i] = M_SPLIT;
    }
    state->id_table[modi - 1] = M_UNCOMP;

    state->block = (uint32_t *)malloc(strm->block_size * sizeof(uint32_t));
    if (state->block == NULL)
    {
        return AE_MEM_ERROR;
    }
    strm->total_in = 0;
    strm->total_out = 0;
    state->xmin = 0;
    state->xmax = (1ULL << strm->bit_per_sample) - 1;

    state->bitp = 0;
    state->mode = M_ID;
    return AE_OK;
}

#define ASK(n)                                          \
    do {                                                \
        while (state->bitp < (unsigned)(n))             \
        {                                               \
            if (strm->avail_in == 0) goto req_buffer;   \
            strm->avail_in--;                           \
            strm->total_in++;                           \
            state->acc <<= 8;                           \
            state->acc |= (uint64_t)(*strm->next_in++); \
            state->bitp += 8;                           \
        }                                               \
    } while (0)

#define GET(n)                                                  \
    ((state->acc >> (state->bitp - (n))) & ((1ULL << (n)) - 1))

#define DROP(n) state->bitp -= (unsigned)(n)

#define ASKFS()                          \
    do {                                 \
        ASK(1);                          \
        state->fs = 0;                   \
        while (GET(state->fs + 1) == 0)  \
        {                                \
            state->fs++;                 \
            ASK(state->fs + 1);          \
        }                                \
    } while(0)

#define GETFS() state->fs

#define DROPFS() DROP(state->fs + 1)

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


int ae_decode(ae_streamp strm, int flush)
{
    /**
       Finite-state machine implementation of the adaptive entropy
       decoder.

       Can work with one byte input und one sample output buffers. If
       enough buffer space is available, then faster implementations
       of the states are called. Inspired by zlib.

       TODO: Flush modes like in zlib
    */

    size_t zero_blocks;
    uint32_t gamma, beta, ms, delta1;
    int k;
    decode_state *state;

    state = strm->state;

    for (;;)
    {
        switch(state->mode)
        {
        case M_ID:
            ASK(3);
            state->id = GET(3);
            DROP(3);
            state->mode = state->id_table[state->id];
            break;

        case M_SPLIT:
            if (SAFE)
            {
                fast_split(strm);
                state->mode = M_ID;
                break;
            }

            if (REFBLOCK)
            {
                COPYSAMPLE();
                state->n = strm->block_size - 1;
            }
            else
            {
                state->n = strm->block_size;
            }

            state->i = state->n;
            state->mode = M_SPLIT_FS;

        case M_SPLIT_FS:
            k = state->id - 1;
            do
            {
                ASKFS();
                state->block[state->i] = GETFS() << k;
                DROPFS();
            }
            while(--state->i);

            state->i = state->n;
            state->mode = M_SPLIT_OUTPUT;

        case M_SPLIT_OUTPUT:
            k = state->id - 1;
            do
            {
                ASK(k);
                PUT(state->block[state->i] + GET(k));
                DROP(k);
            }
            while(--state->i);

            state->mode = M_ID;
            break;

        case M_LOW_ENTROPY:
            ASK(1);
            state->id = GET(1);
            DROP(1);
            state->mode = M_LOW_ENTROPY_REF;

        case M_LOW_ENTROPY_REF:
            if (REFBLOCK)
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
                zero_blocks =  strm->segment_size - (
                    (strm->total_out / strm->block_size)
                    % strm->segment_size);
            }


            if (REFBLOCK)
                state->i = zero_blocks * strm->block_size - 1;
            else
                state->i = zero_blocks * strm->block_size;

            if (strm->avail_out >= state->i)
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
            state->i = REFBLOCK? 1: 0;

        case M_SE_DECODE:
            while(state->i < strm->bit_per_sample)
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
            return AE_STREAM_ERROR;
        }
    }

req_buffer:
    return AE_OK;
}
