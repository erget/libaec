#include <inttypes.h>
#include "libae.h"
#include "aee.h"
#include "aee_mutators.h"

int64_t get_lsb_32(ae_streamp strm)
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

int64_t get_lsb_16(ae_streamp strm)
{
    int64_t data;

    data = (strm->next_in[1] << 8) | strm->next_in[0];

    strm->next_in += 2;
    strm->total_in += 2;
    strm->avail_in -= 2;
    return data;
}

int64_t get_msb_32(ae_streamp strm)
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

int64_t get_msb_16(ae_streamp strm)
{
    int64_t data;

    data = (strm->next_in[0] << 8) | strm->next_in[1];

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
    int64_t *block = strm->state->in_block;

    block[0] = (strm->next_in[0] << 8) | strm->next_in[1];
    block[1] = (strm->next_in[2] << 8) | strm->next_in[3];
    block[2] = (strm->next_in[4] << 8) | strm->next_in[5];
    block[3] = (strm->next_in[6] << 8) | strm->next_in[7];
    block[4] = (strm->next_in[8] << 8) | strm->next_in[9];
    block[5] = (strm->next_in[10] << 8) | strm->next_in[11];
    block[6] = (strm->next_in[12] << 8) | strm->next_in[13];
    block[7] = (strm->next_in[14] << 8) | strm->next_in[15];

    strm->next_in += 16;
    strm->total_in += 16;
    strm->avail_in -= 16;
}

void get_block_msb_16(ae_streamp strm)
{
    int i;
    int64_t *block = strm->state->in_block;

    for (i = 0; i < strm->block_size; i++)
    {
        block[i] = (strm->next_in[2 * i] << 8)
            | strm->next_in[2 * i + 1];
    }
    strm->next_in += 2 * strm->block_size;
    strm->total_in += 2 * strm->block_size;
    strm->avail_in -= 2 * strm->block_size;
}

void get_block_msb_32(ae_streamp strm)
{
    int i;
    int64_t *block = strm->state->in_block;

    for (i = 0; i < strm->block_size; i++)
    {
        block[i] = (strm->next_in[4 * i] << 24)
            | (strm->next_in[4 * i + 1] << 16)
            | (strm->next_in[4 * i + 2] << 8)
            | strm->next_in[4 * i + 3];
    }
    strm->next_in += 4 * strm->block_size;
    strm->total_in += 4 * strm->block_size;
    strm->avail_in -= 4 * strm->block_size;
}

void get_block_8_bs_8(ae_streamp strm)
{
    int64_t *block = strm->state->in_block;

    block[0] = strm->next_in[0];
    block[1] = strm->next_in[1];
    block[2] = strm->next_in[2];
    block[3] = strm->next_in[3];
    block[4] = strm->next_in[4];
    block[5] = strm->next_in[5];
    block[6] = strm->next_in[6];
    block[7] = strm->next_in[7];

    strm->next_in += 8;
    strm->total_in += 8;
    strm->avail_in -= 8;
}

void get_block_8(ae_streamp strm)
{
    int i;
    int64_t *block = strm->state->in_block;

    for (i = 0; i < strm->block_size; i++)
        block[i] = strm->next_in[i];

    strm->next_in += strm->block_size;
    strm->total_in += strm->block_size;
    strm->avail_in -= strm->block_size;
}
