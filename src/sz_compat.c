#include <stdio.h>
#include <stddef.h>
#include "szlib.h"

int SZ_BufftoBuffCompress(void *dest, size_t *destLen, const void *source, size_t sourceLen, SZ_com_t *param)
{
    int status;
    aec_stream strm;

    strm.bit_per_sample = param->bits_per_pixel;
    strm.block_size = param->pixels_per_block;
    strm.rsi = param->pixels_per_scanline / param->pixels_per_block;
    strm.flags = param->options_mask;
    strm.avail_in = sourceLen;
    strm.avail_out = *destLen;
    strm.next_out = dest;
    strm.next_in = source;

    if ((status = aec_encode_init(&strm)) != AEC_OK)
        return status;

    if ((status = aec_encode(&strm, AEC_FLUSH)) != AEC_OK)
        return status;

    *destLen = strm.total_out;

    if ((status = aec_encode_end(&strm)) != AEC_OK)
        return status;

    return SZ_OK;
}

int SZ_BufftoBuffDecompress(void *dest, size_t *destLen, const void *source, size_t sourceLen, SZ_com_t *param)
{
    int status;
    aec_stream strm;

    strm.bit_per_sample = param->bits_per_pixel;
    strm.block_size = param->pixels_per_block;
    strm.rsi = param->pixels_per_scanline / param->pixels_per_block;
    strm.flags = param->options_mask;
    strm.avail_in = sourceLen;
    strm.avail_out = *destLen;
    strm.next_out = dest;
    strm.next_in = source;

    if ((status = aec_decode_init(&strm)) != AEC_OK)
        return status;

    if ((status = aec_decode(&strm, AEC_FLUSH)) != AEC_OK)
        return status;

    *destLen = strm.total_out;

    if ((status = aec_decode_end(&strm)) != AEC_OK)
        return status;

    return SZ_OK;
}
