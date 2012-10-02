#include <stdio.h>
#include <stddef.h>
#include "szlib.h"
#include "libaec.h"

int SZ_BufftoBuffCompress(void *dest, size_t *destLen,
                          const void *source, size_t sourceLen,
                          SZ_com_t *param)
{
    int status;
    struct aec_stream strm;

    strm.bit_per_sample = param->bits_per_pixel;
    strm.block_size = param->pixels_per_block;
    strm.rsi = param->pixels_per_scanline / param->pixels_per_block;
    strm.flags = param->options_mask;
    strm.avail_in = sourceLen;
    strm.avail_out = *destLen;
    strm.next_out = dest;
    strm.next_in = source;

    status = aec_buf_encode(&strm);
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
    strm.flags = param->options_mask;
    strm.avail_in = sourceLen;
    strm.avail_out = *destLen;
    strm.next_out = dest;
    strm.next_in = source;

    status = aec_buf_decode(&strm);
    if (status != AEC_OK)
        return status;

    *destLen = strm.total_out;
    return SZ_OK;
}
