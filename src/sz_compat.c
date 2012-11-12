#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
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

static void interleave_buffer(unsigned char *dest, unsigned char *src,
                              size_t n, int wordsize)
{
    size_t i, j;

    for (i = 0; i < n / wordsize; i++)
        for (j = 0; j < wordsize; j++)
            dest[j * (n / wordsize) + i] = src[i * wordsize + j];
}

static void deinterleave_buffer(unsigned char *dest, unsigned char *src,
                              size_t n, int wordsize)
{
    size_t i, j;

    for (i = 0; i < n / wordsize; i++)
        for (j = 0; j < wordsize; j++)
            dest[i * wordsize + j] = src[j * (n / wordsize) + i];
}

int SZ_BufftoBuffCompress(void *dest, size_t *destLen,
                          const void *source, size_t sourceLen,
                          SZ_com_t *param)
{
    int status;
    struct aec_stream strm;
    unsigned char *buf;

    if (param->bits_per_pixel == 32 || param->bits_per_pixel == 64) {
        buf = (unsigned char *)malloc(sourceLen);
        if (buf == NULL)
            return SZ_MEM_ERROR;

        interleave_buffer(buf, source, sourceLen, param->bits_per_pixel / 8);
        strm.bit_per_sample = 8;
        strm.next_in = buf;
    } else {
        strm.next_in = source;
        strm.bit_per_sample = param->bits_per_pixel;
    }

    strm.avail_in = sourceLen;
    strm.block_size = param->pixels_per_block;
    strm.rsi = param->pixels_per_scanline / param->pixels_per_block;
    strm.flags = convert_options(param->options_mask);
    strm.avail_out = *destLen;
    strm.next_out = dest;

    status = aec_buffer_encode(&strm);
    if (status != AEC_OK)
        return status;

    if (param->bits_per_pixel == 32 || param->bits_per_pixel == 64) {
        free(buf);
    }

    *destLen = strm.total_out;
    return SZ_OK;
}

int SZ_BufftoBuffDecompress(void *dest, size_t *destLen,
                            const void *source, size_t sourceLen,
                            SZ_com_t *param)
{
    int status;
    struct aec_stream strm;
    unsigned char *buf;

    if (param->bits_per_pixel == 32 || param->bits_per_pixel == 64) {
        buf = (unsigned char *)malloc(*destLen);
        if (buf == NULL)
            return SZ_MEM_ERROR;

        strm.bit_per_sample = 8;
        strm.next_out = buf;
    } else {
        strm.next_out = dest;
        strm.bit_per_sample = param->bits_per_pixel;
    }

    strm.block_size = param->pixels_per_block;
    strm.rsi = param->pixels_per_scanline / param->pixels_per_block;
    strm.flags = convert_options(param->options_mask);
    strm.avail_in = sourceLen;
    strm.avail_out = *destLen;
    strm.next_in = source;

    status = aec_buffer_decode(&strm);
    if (status != AEC_OK)
        return status;

    *destLen = strm.total_out;

    if (param->bits_per_pixel == 32 || param->bits_per_pixel == 64) {
        deinterleave_buffer(dest, buf, *destLen, param->bits_per_pixel / 8);
        free(buf);
    }

    return SZ_OK;
}

int SZ_encoder_enabled(void)
{
    return 1;
}
