#include <inttypes.h>
#include "libae.h"
#include "aee.h"
#include "aee_mutators.h"

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

uint32_t get_8(ae_streamp strm)
{
    strm->avail_in--;
    strm->total_in++;
    return *strm->next_in++;
}

void get_block_msb_16_bs_8(ae_streamp strm)
{
    int i;
    uint32_t *block = strm->state->block_buf;

    for (i = 0; i < 8 * strm->rsi; i += 8)
    {
        block[i + 0] = ((uint32_t)strm->next_in[0] << 8) | (uint32_t)strm->next_in[1];
        block[i + 1] = ((uint32_t)strm->next_in[2] << 8) | (uint32_t)strm->next_in[3];
        block[i + 2] = ((uint32_t)strm->next_in[4] << 8) | (uint32_t)strm->next_in[5];
        block[i + 3] = ((uint32_t)strm->next_in[6] << 8) | (uint32_t)strm->next_in[7];
        block[i + 4] = ((uint32_t)strm->next_in[8] << 8) | (uint32_t)strm->next_in[9];
        block[i + 5] = ((uint32_t)strm->next_in[10] << 8) | (uint32_t)strm->next_in[11];
        block[i + 6] = ((uint32_t)strm->next_in[12] << 8) | (uint32_t)strm->next_in[13];
        block[i + 7] = ((uint32_t)strm->next_in[14] << 8) | (uint32_t)strm->next_in[15];

        strm->next_in += 16;
    }
    strm->total_in += 16 * strm->rsi;
    strm->avail_in -= 16 * strm->rsi;
}

void get_block_msb_16(ae_streamp strm)
{
    int i;
    uint32_t *block = strm->state->block_buf;

    for (i = 0; i < strm->block_size * strm->rsi; i++)
    {
        block[i] = ((uint32_t)strm->next_in[2 * i] << 8)
            | (uint32_t)strm->next_in[2 * i + 1];
    }
    strm->next_in += 2 * strm->block_size * strm->rsi;
    strm->total_in += 2 * strm->block_size * strm->rsi;
    strm->avail_in -= 2 * strm->block_size * strm->rsi;
}

void get_block_msb_32(ae_streamp strm)
{
    int i;
    uint32_t *block = strm->state->block_buf;

    for (i = 0; i < strm->block_size * strm->rsi; i++)
    {
        block[i] = ((uint32_t)strm->next_in[4 * i] << 24)
            | ((uint32_t)strm->next_in[4 * i + 1] << 16)
            | ((uint32_t)strm->next_in[4 * i + 2] << 8)
            | (uint32_t)strm->next_in[4 * i + 3];
    }
    strm->next_in += 4 * strm->block_size * strm->rsi;
    strm->total_in += 4 * strm->block_size * strm->rsi;
    strm->avail_in -= 4 * strm->block_size * strm->rsi;
}

void get_block_8_bs_8(ae_streamp strm)
{
    int i;
    uint32_t *block = strm->state->block_buf;

    for (i = 0; i < 8 * strm->rsi; i += 8)
    {
        block[i + 0] = strm->next_in[i + 0];
        block[i + 1] = strm->next_in[i + 1];
        block[i + 2] = strm->next_in[i + 2];
        block[i + 3] = strm->next_in[i + 3];
        block[i + 4] = strm->next_in[i + 4];
        block[i + 5] = strm->next_in[i + 5];
        block[i + 6] = strm->next_in[i + 6];
        block[i + 7] = strm->next_in[i + 7];
    }
    strm->next_in += 8 * strm->rsi;
    strm->total_in += 8 * strm->rsi;
    strm->avail_in -= 8 * strm->rsi;
}

void get_block_8(ae_streamp strm)
{
    int i;
    uint32_t *block = strm->state->block_buf;

    for (i = 0; i < strm->block_size * strm->rsi; i++)
        block[i] = strm->next_in[i];

    strm->next_in += strm->block_size * strm->rsi;
    strm->total_in += strm->block_size * strm->rsi;
    strm->avail_in -= strm->block_size * strm->rsi;
}
