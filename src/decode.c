/* Adaptive Entropy Decoder            */
/* CCSDS 121.0-B-1 and CCSDS 120.0-G-2 */

#include <config.h>

#if HAVE_STDINT_H
# include <stdint.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "libaec.h"
#include "decode.h"

static void put_msb_32(struct aec_stream *strm, int64_t data)
{
    *strm->next_out++ = data >> 24;
    *strm->next_out++ = data >> 16;
    *strm->next_out++ = data >> 8;
    *strm->next_out++ = data;
    strm->avail_out -= 4;
    strm->total_out += 4;
}

static void put_msb_24(struct aec_stream *strm, int64_t data)
{
    *strm->next_out++ = data >> 16;
    *strm->next_out++ = data >> 8;
    *strm->next_out++ = data;
    strm->avail_out -= 3;
    strm->total_out += 3;
}

static void put_msb_16(struct aec_stream *strm, int64_t data)
{
    *strm->next_out++ = data >> 8;
    *strm->next_out++ = data;
    strm->avail_out -= 2;
    strm->total_out += 2;
}

static void put_lsb_32(struct aec_stream *strm, int64_t data)
{
    *strm->next_out++ = data;
    *strm->next_out++ = data >> 8;
    *strm->next_out++ = data >> 16;
    *strm->next_out++ = data >> 24;
    strm->avail_out -= 4;
    strm->total_out += 4;
}

static void put_lsb_24(struct aec_stream *strm, int64_t data)
{
    *strm->next_out++ = data;
    *strm->next_out++ = data >> 8;
    *strm->next_out++ = data >> 16;
    strm->avail_out -= 3;
    strm->total_out += 3;
}

static void put_lsb_16(struct aec_stream *strm, int64_t data)
{
    *strm->next_out++ = data;
    *strm->next_out++ = data >> 8;
    strm->avail_out -= 2;
    strm->total_out += 2;
}

static void put_8(struct aec_stream *strm, int64_t data)
{
    *strm->next_out++ = data;
    strm->avail_out--;
    strm->total_out++;
}

static inline void u_put(struct aec_stream *strm, int64_t sample)
{
    int64_t x, d, th, D, lower, m;
    struct internal_state *state = strm->state;

    if (state->pp && (state->samples_out % state->ref_int != 0)) {
        d = sample;
        x = state->last_out;
        lower = x - state->xmin;
        th = MIN(lower, state->xmax - x);

        if (d <= 2 * th) {
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
    } else {
        if (strm->flags & AEC_DATA_SIGNED) {
            m = 1ULL << (strm->bit_per_sample - 1);
            /* Reference samples have to be sign extended */
            sample = (sample ^ m) - m;
        }
    }
    state->last_out = sample;
    state->put_sample(strm, sample);
    state->samples_out++;
}

static inline int64_t u_get(struct aec_stream *strm, unsigned int n)
{
    /**
       Unsafe get n bit from input stream

       No checking whatsoever. Read bits are dumped.
     */

    struct internal_state *state;

    state = strm->state;
    while (state->bitp < n) {
        strm->avail_in--;
        strm->total_in++;
        state->acc = (state->acc << 8) | *strm->next_in++;
        state->bitp += 8;
    }
    state->bitp -= n;
    return (state->acc >> state->bitp) & ((1ULL << n) - 1);
}

static inline int64_t u_get_fs(struct aec_stream *strm)
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

static inline void fast_split(struct aec_stream *strm)
{
    int i, k;
    struct internal_state *state= strm->state;

    k = state->id - 1;

    if (state->ref)
        u_put(strm, u_get(strm, strm->bit_per_sample));

    for (i = state->ref; i < strm->block_size; i++)
        state->block[i] = u_get_fs(strm) << k;

    for (i = state->ref; i < strm->block_size; i++) {
        state->block[i] += u_get(strm, k);
        u_put(strm, state->block[i]);
    }
}

static inline void fast_zero(struct aec_stream *strm)
{
    int i = strm->state->i;

    while (i--)
        u_put(strm, 0);
}

static inline void fast_se(struct aec_stream *strm)
{
    int i;
    int64_t m, d1;
    struct internal_state *state= strm->state;

    i = state->ref;

    while (i < strm->block_size) {
        m = u_get_fs(strm);
        d1 = m - state->se_table[2 * m + 1];

        if ((i & 1) == 0) {
            u_put(strm, state->se_table[2 * m] - d1);
            i++;
        }
        u_put(strm, d1);
        i++;
    }
}

static inline void fast_uncomp(struct aec_stream *strm)
{
    int i;

    for (i = 0; i < strm->block_size; i++)
        u_put(strm, u_get(strm, strm->bit_per_sample));
}

#define ASK(strm, n)                                     \
    do {                                                 \
        while (strm->state->bitp < (unsigned)(n)) {      \
            if (strm->avail_in == 0)                     \
                return M_EXIT;                           \
            strm->avail_in--;                            \
            strm->total_in++;                            \
            strm->state->acc <<= 8;                      \
            strm->state->acc |= *strm->next_in++;        \
            strm->state->bitp += 8;                      \
        }                                                \
    } while (0)

#define GET(strm, n)                                                    \
    ((strm->state->acc >> (strm->state->bitp - (n))) & ((1ULL << (n)) - 1))

#define DROP(strm, n) strm->state->bitp -= (unsigned)(n)

#define ASKFS(strm)                                                           \
    do {                                                                      \
        ASK(strm, 1);                                                         \
        while ((strm->state->acc & (1ULL << (strm->state->bitp - 1))) == 0) { \
            if (strm->state->bitp == 1) {                                     \
                if (strm->avail_in == 0)                                      \
                    return M_EXIT;                                            \
                strm->avail_in--;                                             \
                strm->total_in++;                                             \
                strm->state->acc <<= 8;                                       \
                strm->state->acc |= *strm->next_in++;                         \
                strm->state->bitp += 8;                                       \
            }                                                                 \
            strm->state->fs++;                                                \
            strm->state->bitp--;                                              \
        }                                                                     \
    } while (0)

#define GETFS(strm) state->fs

#define DROPFS(strm)                            \
    do {                                        \
        strm->state->fs = 0;                    \
        /* Needs to be here for                 \
           ASK/GET/PUT/DROP interleaving. */    \
        strm->state->bitp--;                    \
    } while (0)

#define PUT(strm, sample)                          \
    do {                                           \
        if (strm->avail_out == 0)                  \
            return M_EXIT;                         \
        u_put(strm, (sample));                     \
    } while (0)

#define COPYSAMPLE(strm)                            \
    do {                                            \
        ASK(strm, strm->bit_per_sample);            \
        PUT(strm, GET(strm, strm->bit_per_sample)); \
        DROP(strm, strm->bit_per_sample);           \
    } while (0)


static int m_id(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    if (state->pp && (state->samples_out / strm->block_size) % strm->rsi == 0)
        state->ref = 1;
    else
        state->ref = 0;

    ASK(strm, state->id_len);
    state->id = GET(strm, state->id_len);
    DROP(strm, state->id_len);
    state->mode = state->id_table[state->id];

    return M_CONTINUE;
}

static int m_split_output(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;
    int k = state->id - 1;

    do {
        ASK(strm, k);
        PUT(strm, (state->block[state->i] << k) + GET(strm, k));
        DROP(strm, k);
    } while(state->i--);

    state->mode = m_id;
    return M_CONTINUE;
}

static int m_split_fs(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    do {
        ASKFS(strm);
        state->block[state->i] = GETFS(strm);
        DROPFS(strm);
    } while(state->i--);

    state->i = state->n - 1;
    state->mode = m_split_output;
    return M_CONTINUE;
}

static int m_split(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    if (SAFE) {
        fast_split(strm);
        state->mode = m_id;
        return M_CONTINUE;
    }

    if (state->ref) {
        COPYSAMPLE(strm);
        state->n = strm->block_size - 1;
    } else {
        state->n = strm->block_size;
    }

    state->i = state->n - 1;
    state->mode = m_split_fs;
    return M_CONTINUE;
}

static int m_zero_output(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    do
        PUT(strm, 0);
    while(--state->i);

    state->mode = m_id;
    return M_CONTINUE;
}

static int m_zero_block(struct aec_stream *strm)
{
    int zero_blocks, b;
    struct internal_state *state = strm->state;

    ASKFS(strm);
    zero_blocks = GETFS(strm) + 1;
    DROPFS(strm);

    if (zero_blocks == ROS) {
        b = (state->samples_out / strm->block_size) % strm->rsi;
        zero_blocks = MIN(strm->rsi - b, 64 - (b % 64));
    } else if (zero_blocks > ROS) {
        zero_blocks--;
    }

    if (state->ref)
        state->i = zero_blocks * strm->block_size - 1;
    else
        state->i = zero_blocks * strm->block_size;

    if (strm->avail_out >= state->i * state->byte_per_sample) {
        fast_zero(strm);
        state->mode = m_id;
        return M_CONTINUE;
    }

    state->mode = m_zero_output;
    return M_CONTINUE;
}

static int m_se_decode(struct aec_stream *strm)
{
    int64_t m, d1;
    struct internal_state *state = strm->state;

    while(state->i < strm->block_size) {
        ASKFS(strm);
        m = GETFS(strm);
        d1 = m - state->se_table[2 * m + 1];

        if ((state->i & 1) == 0) {
            PUT(strm, state->se_table[2 * m] - d1);
            state->i++;
        }

        PUT(strm, d1);
        state->i++;
        DROPFS(strm);
    }

    state->mode = m_id;
    return M_CONTINUE;
}

static int m_se(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    if (SAFE) {
        fast_se(strm);
        state->mode = m_id;
        return M_CONTINUE;
    }

    state->mode = m_se_decode;
    state->i = state->ref;
    return M_CONTINUE;
}

static int m_low_entropy_ref(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    if (state->ref)
        COPYSAMPLE(strm);

    if(state->id == 1) {
        state->mode = m_se;
        return M_CONTINUE;
    }

    state->mode = m_zero_block;
    return M_CONTINUE;
}

static int m_low_entropy(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    ASK(strm, 1);
    state->id = GET(strm, 1);
    DROP(strm, 1);
    state->mode = m_low_entropy_ref;
    return M_CONTINUE;
}

static int m_uncomp_copy(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    do
        COPYSAMPLE(strm);
    while(--state->i);

    state->mode = m_id;
    return M_CONTINUE;
}

static int m_uncomp(struct aec_stream *strm)
{
    struct internal_state *state = strm->state;

    if (SAFE) {
        fast_uncomp(strm);
        state->mode = m_id;
        return M_CONTINUE;
    }

    state->i = strm->block_size;
    state->mode = m_uncomp_copy;
    return M_CONTINUE;
}

static void create_se_table(int *table)
{
    int i, j, k, ms;

    k = 0;
    for (i = 0; i < 13; i++) {
        ms = k;
        for (j = 0; j <= i; j++) {
            table[2 * k] = i;
            table[2 * k + 1] = ms;
            k++;
        }
    }
}

int aec_decode_init(struct aec_stream *strm)
{
    int i, modi;
    struct internal_state *state;

    if (strm->bit_per_sample > 32 || strm->bit_per_sample == 0)
        return AEC_CONF_ERROR;

    state = (struct internal_state *) malloc(sizeof(struct internal_state));
    if (state == NULL)
        return AEC_MEM_ERROR;

    state->se_table = (int *) malloc(180 * sizeof(int));
    if (state->se_table == NULL)
        return AEC_MEM_ERROR;

    create_se_table(state->se_table);

    strm->state = state;

    if (strm->bit_per_sample > 16) {
        state->id_len = 5;

        if (strm->bit_per_sample <= 24 && strm->flags & AEC_DATA_3BYTE) {
            state->byte_per_sample = 3;
            if (strm->flags & AEC_DATA_MSB)
                state->put_sample = put_msb_24;
            else
                state->put_sample = put_lsb_24;
        } else {
            state->byte_per_sample = 4;
            if (strm->flags & AEC_DATA_MSB)
                state->put_sample = put_msb_32;
            else
                state->put_sample = put_lsb_32;
        }
        state->out_blklen = strm->block_size
            * state->byte_per_sample;
    }
    else if (strm->bit_per_sample > 8) {
        state->byte_per_sample = 2;
        state->id_len = 4;
        state->out_blklen = strm->block_size * 2;
        if (strm->flags & AEC_DATA_MSB)
            state->put_sample = put_msb_16;
        else
            state->put_sample = put_lsb_16;
    } else {
        state->byte_per_sample = 1;
        state->id_len = 3;
        state->out_blklen = strm->block_size;
        state->put_sample = put_8;
    }

    if (strm->flags & AEC_DATA_SIGNED) {
        state->xmin = -(1ULL << (strm->bit_per_sample - 1));
        state->xmax = (1ULL << (strm->bit_per_sample - 1)) - 1;
    } else {
        state->xmin = 0;
        state->xmax = (1ULL << strm->bit_per_sample) - 1;
    }

    state->ref_int = strm->block_size * strm->rsi;
    state->in_blklen = (strm->block_size * strm->bit_per_sample
                        + state->id_len) / 8 + 1;

    modi = 1UL << state->id_len;
    state->id_table = malloc(modi * sizeof(int (*)(struct aec_stream *)));
    if (state->id_table == NULL)
        return AEC_MEM_ERROR;

    state->id_table[0] = m_low_entropy;
    for (i = 1; i < modi - 1; i++) {
        state->id_table[i] = m_split;
    }
    state->id_table[modi - 1] = m_uncomp;

    state->block = (int64_t *)malloc(strm->block_size * sizeof(int64_t));
    if (state->block == NULL)
        return AEC_MEM_ERROR;

    strm->total_in = 0;
    strm->total_out = 0;

    state->samples_out = 0;
    state->bitp = 0;
    state->fs = 0;
    state->pp = strm->flags & AEC_DATA_PREPROCESS;
    state->mode = m_id;
    return AEC_OK;
}

int aec_decode(struct aec_stream *strm, int flush)
{
    /**
       Finite-state machine implementation of the adaptive entropy
       decoder.

       Can work with one byte input und one sample output buffers. If
       enough buffer space is available, then faster implementations
       of the states are called. Inspired by zlib.
    */

    while (strm->state->mode(strm) == M_CONTINUE);
    return AEC_OK;
}

int aec_decode_end(struct aec_stream *strm)
{
    struct internal_state *state= strm->state;

    free(state->block);
    free(state->id_table);
    free(state);
    return AEC_OK;
}

int aec_buffer_decode(struct aec_stream *strm)
{
    int status;

    status = aec_decode_init(strm);
    if (status != AEC_OK)
        return status;

    status = aec_decode(strm, AEC_FLUSH);
    aec_decode_end(strm);
    return status;
}
