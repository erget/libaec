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
        if (sz_opts & i)
            opts |= co[i];

    return opts;
}

static int bits_to_bytes(int bit_length)
{
    if (bit_length > 16)
        return 4;
    else if (bit_length > 8)
        return 2;
    else
        return 1;
}

static void interleave_buffer(void *dest, const void *src,
                              size_t n, int wordsize)
{
    size_t i, j;
    const unsigned char *src8;
    unsigned char *dest8;

    src8 = (unsigned char *)src;
    dest8 = (unsigned char *)dest;

    for (i = 0; i < n / wordsize; i++)
        for (j = 0; j < wordsize; j++)
            dest8[j * (n / wordsize) + i] = src8[i * wordsize + j];
}

static void deinterleave_buffer(void *dest, const void *src,
                                size_t n, int wordsize)
{
    size_t i, j;
    const unsigned char *src8;
    unsigned char *dest8;

    src8 = (unsigned char *)src;
    dest8 = (unsigned char *)dest;

    for (i = 0; i < n / wordsize; i++)
        for (j = 0; j < wordsize; j++)
            dest8[i * wordsize + j] = src8[j * (n / wordsize) + i];
}

static size_t add_padding(void *dest, const void *src, size_t total,
                          size_t line_size, size_t padding_size,
                          int pixel_size, int pp)
{
    size_t i, j, k;
    const char *pixel;
    const char zero_pixel[] = {0, 0, 0, 0, 0, 0, 0, 0};

    for (i = 0, j = 0;
         i < total;
         i += pixel_size, j += pixel_size) {
        if (i > 0 && (i % line_size) == 0) {
            if (pp)
                pixel = (char *)src + i - 1;
            else
                pixel = zero_pixel;
            for (k = 0; k < padding_size; k += pixel_size)
                memcpy((char *)dest + j + k, pixel, pixel_size);
            j += padding_size;
        }
        memcpy((char *)dest + j, (char *)src + i, pixel_size);
    }
    return j;
}

static size_t remove_padding(void *buf, size_t total,
                             size_t line_size, size_t padding_size,
                             int pixel_size)
{
    size_t i, j;

    for (i = 0, j = padding_size;
         i < total;
         i += pixel_size, j += pixel_size) {
        if (i % (line_size + padding_size) == 0)
            j -= padding_size;
        memcpy((char *)buf + j, (char *)buf + i, pixel_size);
    }
    if (i % (line_size + padding_size) == 0)
        j -= padding_size;
    return j;
}

int SZ_BufftoBuffCompress(void *dest, size_t *destLen,
                          const void *source, size_t sourceLen,
                          SZ_com_t *param)
{
    struct aec_stream strm;
    int status;
    void *padbuf = 0;
    void *buf = 0;
    size_t padding_size;
    size_t padded_length;
    size_t scanlines;
    size_t buf_size;
    int pixel_size;
    int pad_scanline;
    int interleave;

    strm.block_size = param->pixels_per_block;
    strm.rsi = (param->pixels_per_scanline + param->pixels_per_block - 1)
        / param->pixels_per_block;
    strm.flags = convert_options(param->options_mask);
    strm.avail_out = *destLen;
    strm.next_out = dest;

    pad_scanline = param->pixels_per_scanline % param->pixels_per_block;
    interleave = param->bits_per_pixel == 32 || param->bits_per_pixel == 64;

    if (interleave) {
        strm.bits_per_sample = 8;
        buf = malloc(sourceLen);
        if (buf == NULL)
            return SZ_MEM_ERROR;
        interleave_buffer(buf, source, sourceLen, param->bits_per_pixel / 8);
    } else {
        strm.bits_per_sample = param->bits_per_pixel;
        buf = (void *)source;
    }

    pixel_size = bits_to_bytes(strm.bits_per_sample);

    if (pad_scanline) {
        scanlines = (sourceLen / pixel_size + param->pixels_per_scanline - 1)
            / param->pixels_per_scanline;
        buf_size = strm.rsi * param->pixels_per_block * pixel_size * scanlines;

        padbuf = malloc(buf_size);
        if (padbuf == NULL)
            return SZ_MEM_ERROR;

        padding_size = (
            param->pixels_per_block -
            (param->pixels_per_scanline % param->pixels_per_block)
            ) * pixel_size;

        padded_length = add_padding(padbuf, buf, sourceLen,
                                    param->pixels_per_scanline * pixel_size,
                                    padding_size, pixel_size,
                                    strm.flags & AEC_DATA_PREPROCESS);

        strm.next_in = padbuf;
        strm.avail_in = padded_length;
    } else {
        strm.next_in = buf;
        strm.avail_in = sourceLen;
    }

    status = aec_buffer_encode(&strm);
    if (status != AEC_OK)
        return status;

    *destLen = strm.total_out;

    if (pad_scanline && padbuf)
        free(padbuf);

    if (interleave && buf)
        free(buf);

    return SZ_OK;
}

int SZ_BufftoBuffDecompress(void *dest, size_t *destLen,
                            const void *source, size_t sourceLen,
                            SZ_com_t *param)
{
    struct aec_stream strm;
    int status;
    void *buf = 0;
    size_t padding_size;
    size_t scanlines;
    size_t buf_size, total_out;
    int pixel_size;
    int pad_scanline;
    int deinterleave;
    int extra_buffer;

    strm.block_size = param->pixels_per_block;
    strm.rsi = (param->pixels_per_scanline + param->pixels_per_block - 1)
        / param->pixels_per_block;
    strm.flags = convert_options(param->options_mask);
    strm.avail_in = sourceLen;
    strm.next_in = source;

    pad_scanline = param->pixels_per_scanline % param->pixels_per_block;
    deinterleave = param->bits_per_pixel == 32 || param->bits_per_pixel == 64;
    extra_buffer = pad_scanline || deinterleave;

    if (deinterleave)
        strm.bits_per_sample = 8;
    else
        strm.bits_per_sample = param->bits_per_pixel;

    pixel_size = bits_to_bytes(strm.bits_per_sample);

    if (extra_buffer) {
        if (pad_scanline) {
            scanlines = (*destLen / pixel_size + param->pixels_per_scanline - 1)
                / param->pixels_per_scanline;
            buf_size = strm.rsi * param->pixels_per_block
                * pixel_size * scanlines;
        } else {
            buf_size = *destLen;
        }
        buf = malloc(buf_size);
        if (buf == NULL)
            return SZ_MEM_ERROR;
        strm.next_out = buf;
        strm.avail_out = buf_size;
    } else {
        strm.next_out = dest;
        strm.avail_out = *destLen;
    }

    status = aec_buffer_decode(&strm);
    if (status != AEC_OK)
        return status;

    if (pad_scanline) {
        padding_size = (
            param->pixels_per_block -
            (param->pixels_per_scanline % param->pixels_per_block)
            ) * pixel_size;
        total_out = remove_padding(buf, strm.total_out,
                                   param->pixels_per_scanline * pixel_size,
                                   padding_size, pixel_size);
    } else {
        total_out = strm.total_out;
    }

    if (total_out < *destLen)
        *destLen = total_out;

    if (deinterleave)
        deinterleave_buffer(dest, buf, *destLen, param->bits_per_pixel / 8);
    else if (pad_scanline)
        memcpy(dest, buf, *destLen);

    if (extra_buffer && buf)
        free(buf);

    return SZ_OK;
}

int SZ_encoder_enabled(void)
{
    return 1;
}
