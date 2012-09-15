#include <inttypes.h>
#include <string.h>
#include "libae.h"
#include "aee.h"
#include "aee_accessors.h"

uint32_t get_8(ae_streamp strm)
{
    strm->avail_in--;
    strm->total_in++;
    return *strm->next_in++;
}

uint32_t get_lsb_24(ae_streamp strm)
{
    uint32_t data;

    data = ((uint32_t)strm->next_in[2] << 16)
        | ((uint32_t)strm->next_in[1] << 8)
        | (uint32_t)strm->next_in[0];

    strm->next_in += 3;
    strm->total_in += 3;
    strm->avail_in -= 3;
    return data;
}

uint32_t get_msb_24(ae_streamp strm)
{
    uint32_t data;

    data = ((uint32_t)strm->next_in[0] << 16)
        | ((uint32_t)strm->next_in[1] << 8)
        | (uint32_t)strm->next_in[2];

    strm->next_in += 3;
    strm->total_in += 3;
    strm->avail_in -= 3;
    return data;
}

#ifdef WORDS_BIGENDIAN

uint32_t get_lsb_16(ae_streamp strm)
{
    uint32_t data;

    data = ((uint32_t)strm->next_in[1] << 8)
        | (uint32_t)strm->next_in[0];

    strm->next_in += 2;
    strm->total_in += 2;
    strm->avail_in -= 2;
    return data;
}

uint32_t get_msb_16(ae_streamp strm)
{
    uint32_t data;

    data = *(uint16_t *)strm->next_in;
    strm->next_in += 2;
    strm->total_in += 2;
    strm->avail_in -= 2;
    return data;
}

uint32_t get_lsb_32(ae_streamp strm)
{
    uint32_t data;

    data = ((uint32_t)strm->next_in[3] << 24)
        | ((uint32_t)strm->next_in[2] << 16)
        | ((uint32_t)strm->next_in[1] << 8)
        | (uint32_t)strm->next_in[0];

    strm->next_in += 4;
    strm->total_in += 4;
    strm->avail_in -= 4;
    return data;
}

uint32_t get_msb_32(ae_streamp strm)
{
    uint32_t data;

    data = *(uint32_t *)strm->next_in;
    strm->next_in += 4;
    strm->total_in += 4;
    strm->avail_in -= 4;
    return data;
}
#else /* LITTLEENDIAN */

uint32_t get_lsb_16(ae_streamp strm)
{
    uint32_t data;

    data = *(uint16_t *)strm->next_in;
    strm->next_in += 2;
    strm->total_in += 2;
    strm->avail_in -= 2;
    return data;
}

uint32_t get_msb_16(ae_streamp strm)
{
    uint32_t data;

    data = ((uint32_t)strm->next_in[0] << 8)
        | (uint32_t)strm->next_in[1];

    strm->next_in += 2;
    strm->total_in += 2;
    strm->avail_in -= 2;
    return data;
}

uint32_t get_lsb_32(ae_streamp strm)
{
    uint32_t data;

    data = *(uint32_t *)strm->next_in;
    strm->next_in += 4;
    strm->total_in += 4;
    strm->avail_in -= 4;
    return data;
}

uint32_t get_msb_32(ae_streamp strm)
{
    uint32_t data;

    data = ((uint32_t)strm->next_in[0] << 24)
        | ((uint32_t)strm->next_in[1] << 16)
        | ((uint32_t)strm->next_in[2] << 8)
        | (uint32_t)strm->next_in[3];

    strm->next_in += 4;
    strm->total_in += 4;
    strm->avail_in -= 4;
    return data;
}
#endif

#define GET_BLOCK_8(BS)                                              \
    static void get_block_8_bs_##BS(ae_streamp strm)                 \
    {                                                                \
        int i, j;                                                    \
        uint32_t *block = strm->state->block_buf;                    \
                                                                     \
        for (i = 0; i < strm->rsi; i++)                              \
            for (j = 0; j < BS; j++)                                 \
                block[i * BS + j] = strm->next_in[i * BS + j];       \
                                                                     \
        strm->next_in += BS * strm->rsi;                             \
        strm->total_in += BS * strm->rsi;                            \
        strm->avail_in -= BS * strm->rsi;                            \
    }

#define GET_BLOCK_NATIVE_16(BS)                                      \
    static void get_block_native_16_bs_##BS(ae_streamp strm)         \
    {                                                                \
        int i, j;                                                    \
        uint32_t *block = strm->state->block_buf;                    \
        uint16_t *next_in = (uint16_t *)strm->next_in;               \
                                                                     \
        for (i = 0; i < strm->rsi; i++)                              \
            for (j = 0; j < BS; j++)                                 \
                block[i * BS + j] = (uint32_t)next_in[i * BS + j];   \
                                                                     \
        strm->next_in += 2 * BS * strm->rsi;                         \
        strm->total_in += 2 * BS * strm->rsi;                        \
        strm->avail_in -= 2 * BS * strm->rsi;                        \
    }

#define GET_BLOCK_LSB_16(BS)                                         \
    static void get_block_lsb_16_bs_##BS(ae_streamp strm)            \
    {                                                                \
        int i, j;                                                    \
        uint32_t *block = strm->state->block_buf;                    \
                                                                     \
        for (i = 0; i < strm->rsi; i++)                              \
            for (j = 0; j < BS; j++)                                 \
                block[i * BS + j] =                                  \
                    (uint32_t)strm->next_in[2 * (i * BS + j)]        \
                    | ((uint32_t)strm->next_in[2 * (i * BS + j) + 1] \
                       << 8);                                        \
                                                                     \
        strm->next_in += 2 * BS * strm->rsi;                         \
        strm->total_in += 2 * BS * strm->rsi;                        \
        strm->avail_in -= 2 * BS * strm->rsi;                        \
    }

#define GET_BLOCK_MSB_16(BS)                                         \
    static void get_block_msb_16_bs_##BS(ae_streamp strm)            \
    {                                                                \
        int i, j;                                                    \
        uint32_t *block = strm->state->block_buf;                    \
                                                                     \
        for (i = 0; i < strm->rsi; i++)                              \
            for (j = 0; j < BS; j++)                                 \
                block[i * BS + j] =                                  \
                    ((uint32_t)strm->next_in[2 * (i * BS + j)] << 8) \
                    | (uint32_t)strm->next_in[2 * (i * BS + j) + 1]; \
                                                                     \
        strm->next_in += 2 * BS * strm->rsi;                         \
        strm->total_in += 2 * BS * strm->rsi;                        \
        strm->avail_in -= 2 * BS * strm->rsi;                        \
    }

#define GET_BLOCK_LSB_24(BS)                                         \
    static void get_block_lsb_24_bs_##BS(ae_streamp strm)            \
    {                                                                \
        int i, j;                                                    \
        uint32_t *block = strm->state->block_buf;                    \
                                                                     \
        for (i = 0; i < strm->rsi; i++)                              \
            for (j = 0; j < BS; j++)                                 \
                block[i * BS + j] =                                  \
                    (uint32_t)strm->next_in[4 * (i * BS + j)]        \
                    | ((uint32_t)strm->next_in[4 * (i * BS + j) + 1] \
                       << 8)                                         \
                    | ((uint32_t)strm->next_in[4 * (i * BS + j) + 2] \
                       << 16);                                       \
                                                                     \
        strm->next_in += 3 * BS * strm->rsi;                         \
        strm->total_in += 3 * BS * strm->rsi;                        \
        strm->avail_in -= 3 * BS * strm->rsi;                        \
    }

#define GET_BLOCK_MSB_24(BS)                                         \
    static void get_block_msb_24_bs_##BS(ae_streamp strm)            \
    {                                                                \
        int i, j;                                                    \
        uint32_t *block = strm->state->block_buf;                    \
                                                                     \
        for (i = 0; i < strm->rsi; i++)                              \
            for (j = 0; j < BS; j++)                                 \
                block[i * BS + j] =                                  \
                    ((uint32_t)strm->next_in[4 * (i * BS + j)]       \
                       << 16)                                        \
                    | ((uint32_t)strm->next_in[4 * (i * BS + j) + 1] \
                       << 8)                                         \
                    | (uint32_t)strm->next_in[4 * (i * BS + j) + 2]; \
                                                                     \
        strm->next_in += 3 * BS * strm->rsi;                         \
        strm->total_in += 3 * BS * strm->rsi;                        \
        strm->avail_in -= 3 * BS * strm->rsi;                        \
    }

#define GET_BLOCK_NATIVE_32(BS)                                      \
    static void get_block_native_32_bs_##BS(ae_streamp strm)         \
    {                                                                \
        memcpy(strm->state->block_buf,                               \
               strm->next_in,                                        \
               4 * BS * strm->rsi);                                  \
                                                                     \
        strm->next_in += 4 * BS * strm->rsi;                         \
        strm->total_in += 4 * BS * strm->rsi;                        \
        strm->avail_in -= 4 * BS * strm->rsi;                        \
    }

#define GET_BLOCK_LSB_32(BS)                                         \
    static void get_block_lsb_32_bs_##BS(ae_streamp strm)            \
    {                                                                \
        int i, j;                                                    \
        uint32_t *block = strm->state->block_buf;                    \
                                                                     \
        for (i = 0; i < strm->rsi; i++)                              \
            for (j = 0; j < BS; j++)                                 \
                block[i * BS + j] =                                  \
                    (uint32_t)strm->next_in[4 * (i * BS + j)]        \
                    | ((uint32_t)strm->next_in[4 * (i * BS + j) + 1] \
                       << 8)                                         \
                    | ((uint32_t)strm->next_in[4 * (i * BS + j) + 2] \
                       << 16)                                        \
                    | ((uint32_t)strm->next_in[4 * (i * BS + j) + 3] \
                       << 24);                                       \
                                                                     \
        strm->next_in += 4 * BS * strm->rsi;                         \
        strm->total_in += 4 * BS * strm->rsi;                        \
        strm->avail_in -= 4 * BS * strm->rsi;                        \
    }

#define GET_BLOCK_MSB_32(BS)                                         \
    static void get_block_msb_32_bs_##BS(ae_streamp strm)            \
    {                                                                \
        int i, j;                                                    \
        uint32_t *block = strm->state->block_buf;                    \
                                                                     \
        for (i = 0; i < strm->rsi; i++)                              \
            for (j = 0; j < BS; j++)                                 \
                block[i * BS + j] =                                  \
                    ((uint32_t)strm->next_in[4 * (i * BS + j)]       \
                     << 24)                                          \
                    | ((uint32_t)strm->next_in[4 * (i * BS + j) + 1] \
                       << 16)                                        \
                    | ((uint32_t)strm->next_in[4 * (i * BS + j) + 2] \
                       << 8)                                         \
                    | (uint32_t)strm->next_in[4 * (i * BS + j) + 3]; \
                                                                     \
        strm->next_in += 4 * BS * strm->rsi;                         \
        strm->total_in += 4 * BS * strm->rsi;                        \
        strm->avail_in -= 4 * BS * strm->rsi;                        \
    }

#define GET_BLOCK_FUNCS(A, B)                           \
    void (*get_block_funcs_##A[])(ae_streamp) = {       \
        get_block_##B##_bs_8,                           \
        get_block_##B##_bs_16,                          \
        get_block_##B##_bs_32,                          \
        get_block_##B##_bs_64,                          \
    }

GET_BLOCK_8(8);
GET_BLOCK_8(16);
GET_BLOCK_8(32);
GET_BLOCK_8(64);

GET_BLOCK_FUNCS(8, 8);

GET_BLOCK_LSB_24(8);
GET_BLOCK_LSB_24(16);
GET_BLOCK_LSB_24(32);
GET_BLOCK_LSB_24(64);

GET_BLOCK_FUNCS(lsb_24, lsb_24);

GET_BLOCK_MSB_24(8);
GET_BLOCK_MSB_24(16);
GET_BLOCK_MSB_24(32);
GET_BLOCK_MSB_24(64);

GET_BLOCK_FUNCS(msb_24, msb_24);

GET_BLOCK_NATIVE_16(8);
GET_BLOCK_NATIVE_16(16);
GET_BLOCK_NATIVE_16(32);
GET_BLOCK_NATIVE_16(64);

GET_BLOCK_NATIVE_32(8);
GET_BLOCK_NATIVE_32(16);
GET_BLOCK_NATIVE_32(32);
GET_BLOCK_NATIVE_32(64);

#ifdef WORDS_BIGENDIAN

GET_BLOCK_LSB_16(8);
GET_BLOCK_LSB_16(16);
GET_BLOCK_LSB_16(32);
GET_BLOCK_LSB_16(64);

GET_BLOCK_LSB_32(8);
GET_BLOCK_LSB_32(16);
GET_BLOCK_LSB_32(32);
GET_BLOCK_LSB_32(64);

GET_BLOCK_FUNCS(lsb_16, lsb_16);
GET_BLOCK_FUNCS(msb_16, native_16);
GET_BLOCK_FUNCS(lsb_32, lsb_32);
GET_BLOCK_FUNCS(msb_32, native_32);

#else /* LITTLEENDIAN */

GET_BLOCK_MSB_16(8);
GET_BLOCK_MSB_16(16);
GET_BLOCK_MSB_16(32);
GET_BLOCK_MSB_16(64);

GET_BLOCK_MSB_32(8);
GET_BLOCK_MSB_32(16);
GET_BLOCK_MSB_32(32);
GET_BLOCK_MSB_32(64);

GET_BLOCK_FUNCS(lsb_16, native_16);
GET_BLOCK_FUNCS(msb_16, msb_16);
GET_BLOCK_FUNCS(lsb_32, native_32);
GET_BLOCK_FUNCS(msb_32, msb_32);

#endif
