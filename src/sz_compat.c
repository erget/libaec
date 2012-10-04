#include <stdio.h>
#include <stddef.h>
#include "szlib.h"
#include "libaec.h"

#define NOPTS 129

static int convert_options(int sz_opts)
{
    int co[NOPTS];
    int i;
    int opts = 0;

    memset(co, 0, sizeof(int) * NOPTS);
    co[SZ_MSB_OPTION_MASK] = AEC_DATA_MSB;
    co[SZ_NN_OPTION_MASK] = AEC_DATA_PREPROCESS;

    for (i = 1; i < NOPTS; i <<= 1)
        opts |= co[i];

    return opts;
}

int SZ_BufftoBuffCompress(void *dest, size_t *destLen,
                          const void *source, size_t sourceLen,
                          SZ_com_t *param)
{
    int status;
    struct aec_stream strm;

    strm.bit_per_sample = param->bits_per_pixel;
    strm.block_size = param->pixels_per_block;
    strm.rsi = param->pixels_per_scanline / param->pixels_per_block;
    strm.flags = convert_options(param->options_mask);
    strm.avail_in = sourceLen;
    strm.avail_out = *destLen;
    strm.next_out = dest;
    strm.next_in = source;

    status = aec_buffer_encode(&strm);
    if (status != AEC_OK)
        return status;

    *destLen = strm.total_out;
    return SZ_OK;
}

int SZ_BufftoBuffDecompress(void *dest, size_t *destLen,
                            const void *source, size_t sourceLen,
                            SZ_com_t *param)
{
    int status;
    struct aec_stream strm;

    strm.bit_per_sample = param->bits_per_pixel;
    strm.block_size = param->pixels_per_block;
    strm.rsi = param->pixels_per_scanline / param->pixels_per_block;
    strm.flags = convert_options(param->options_mask);
    strm.avail_in = sourceLen;
    strm.avail_out = *destLen;
    strm.next_out = dest;
    strm.next_in = source;

    status = aec_buffer_decode(&strm);
    if (status != AEC_OK)
        return status;

    *destLen = strm.total_out;
    return SZ_OK;
}

int SZ_encoder_enabled(void)
{
    return 1;
}
