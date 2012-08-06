#include <stddef.h>
#include "szlib.h"

int SZ_BufftoBuffCompress(void *dest, size_t *destLen, const void *source, size_t sourceLen, SZ_com_t *param)
{
    int status;
    ae_stream strm;

    strm.bit_per_sample = param->bits_per_pixel;
    strm.block_size = param->pixels_per_block;
    strm.segment_size = param->pixels_per_scanline / param->pixels_per_block;
    strm.flags = param->options_mask;
    strm.avail_in = sourceLen;
    strm.avail_out = *destLen;
    strm.next_out = dest;
    strm.next_in = source;

    if ((status = ae_encode_init(&strm)) != AE_OK)
        return status;

    if ((status = ae_encode(&strm, AE_FLUSH)) != AE_OK)
        return status;

    *destLen = strm.total_out;
    return SZ_OK;
}

int SZ_BufftoBuffDecompress(void *dest, size_t *destLen, const void *source, size_t sourceLen, SZ_com_t *param)
{
    int status;
    ae_stream strm;

    strm.bit_per_sample = param->bits_per_pixel;
    strm.block_size = param->pixels_per_block;
    strm.segment_size = param->pixels_per_scanline / param->pixels_per_block;
    strm.flags = param->options_mask;
    strm.avail_in = sourceLen;
    strm.avail_out = *destLen;
    strm.next_out = dest;
    strm.next_in = source;

    if ((status = ae_decode_init(&strm)) != AE_OK)
        return status;

    if ((status = ae_decode(&strm, AE_FLUSH)) != AE_OK)
        return status;

    *destLen = strm.total_out;
    return SZ_OK;
}
