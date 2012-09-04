#include <inttypes.h>
#include "libae.h"
#include "aee.h"
#include "aee_mutators.h"

int64_t get_lsb_32(ae_streamp strm)
{
    int64_t data;

    data = ((int64_t)strm->next_in[3] << 24)
        | ((int64_t)strm->next_in[2] << 16)
        | ((int64_t)strm->next_in[1] << 8)
        | (int64_t)strm->next_in[0];

    strm->next_in += 4;
    strm->total_in += 4;
    strm->avail_in -= 4;
    return data;
}

int64_t get_lsb_16(ae_streamp strm)
{
    int64_t data;

    data = ((int64_t)strm->next_in[1] << 8)
        | (int64_t)strm->next_in[0];

    strm->next_in += 2;
    strm->total_in += 2;
    strm->avail_in -= 2;
    return data;
}

int64_t get_msb_32(ae_streamp strm)
{
    int64_t data;

    data = ((int64_t)strm->next_in[0] << 24)
        | ((int64_t)strm->next_in[1] << 16)
        | ((int64_t)strm->next_in[2] << 8)
        | (int64_t)strm->next_in[3];

    strm->next_in += 4;
    strm->total_in += 4;
    strm->avail_in -= 4;
    return data;
}

int64_t get_msb_16(ae_streamp strm)
{
    int64_t data;

    data = ((int64_t)strm->next_in[0] << 8)
        | (int64_t)strm->next_in[1];

    strm->next_in += 2;
    strm->total_in += 2;
    strm->avail_in -= 2;
    return data;
}

int64_t get_8(ae_streamp strm)
{
    strm->avail_in--;
    strm->total_in++;
    return *strm->next_in++;
}

void get_block_msb_16_bs_8(ae_streamp strm)
{
    int i;
    int64_t *block = strm->state->block_buf;

    for (i = 0; i < 8 * strm->rsi; i += 8)
    {
        block[i + 0] = ((int64_t)strm->next_in[0] << 8) | (int64_t)strm->next_in[1];
        block[i + 1] = ((int64_t)strm->next_in[2] << 8) | (int64_t)strm->next_in[3];
        block[i + 2] = ((int64_t)strm->next_in[4] << 8) | (int64_t)strm->next_in[5];
        block[i + 3] = ((int64_t)strm->next_in[6] << 8) | (int64_t)strm->next_in[7];
        block[i + 4] = ((int64_t)strm->next_in[8] << 8) | (int64_t)strm->next_in[9];
        block[i + 5] = ((int64_t)strm->next_in[10] << 8) | (int64_t)strm->next_in[11];
        block[i + 6] = ((int64_t)strm->next_in[12] << 8) | (int64_t)strm->next_in[13];
        block[i + 7] = ((int64_t)strm->next_in[14] << 8) | (int64_t)strm->next_in[15];

        strm->next_in += 16;
    }
    strm->total_in += 16 * strm->rsi;
    strm->avail_in -= 16 * strm->rsi;
}

void get_block_msb_16(ae_streamp strm)
{
    int i;
    int64_t *block = strm->state->block_buf;

    for (i = 0; i < strm->block_size * strm->rsi; i++)
    {
        block[i] = ((int64_t)strm->next_in[2 * i] << 8)
            | (int64_t)strm->next_in[2 * i + 1];
    }
    strm->next_in += 2 * strm->block_size * strm->rsi;
    strm->total_in += 2 * strm->block_size * strm->rsi;
    strm->avail_in -= 2 * strm->block_size * strm->rsi;
}

void get_block_msb_32(ae_streamp strm)
{
    int i;
    int64_t *block = strm->state->block_buf;

    for (i = 0; i < strm->block_size * strm->rsi; i++)
    {
        block[i] = ((int64_t)strm->next_in[4 * i] << 24)
            | ((int64_t)strm->next_in[4 * i + 1] << 16)
            | ((int64_t)strm->next_in[4 * i + 2] << 8)
            | (int64_t)strm->next_in[4 * i + 3];
    }
    strm->next_in += 4 * strm->block_size * strm->rsi;
    strm->total_in += 4 * strm->block_size * strm->rsi;
    strm->avail_in -= 4 * strm->block_size * strm->rsi;
}

void get_block_8_bs_8(ae_streamp strm)
{
    int i;
    int64_t *block = strm->state->block_buf;

    for (i = 0; i < 8 * strm->rsi; i += 8)
    {
        block[i + 0] = strm->next_in[0];
        block[i + 1] = strm->next_in[1];
        block[i + 2] = strm->next_in[2];
        block[i + 3] = strm->next_in[3];
        block[i + 4] = strm->next_in[4];
        block[i + 5] = strm->next_in[5];
        block[i + 6] = strm->next_in[6];
        block[i + 7] = strm->next_in[7];

        strm->next_in += 8;
        strm->total_in += 8;
        strm->avail_in -= 8;
    }
}

void get_block_8(ae_streamp strm)
{
    int i;
    int64_t *block = strm->state->block_buf;

    for (i = 0; i < strm->block_size * strm->rsi; i++)
        block[i] = strm->next_in[i];

    strm->next_in += strm->block_size * strm->rsi;
    strm->total_in += strm->block_size * strm->rsi;
    strm->avail_in -= strm->block_size * strm->rsi;
}
