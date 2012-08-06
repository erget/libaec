#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include "szlib.h"

#define OPTIONS_MASK        (SZ_RAW_OPTION_MASK | SZ_MSB_OPTION_MASK | SZ_NN_OPTION_MASK)
#define PIXELS_PER_BLOCK    (8)
#define PIXELS_PER_SCANLINE (PIXELS_PER_BLOCK*128)

int main(int argc, char *argv[])
{
    int status, c;
    SZ_com_t sz_param;
    unsigned char *dest;
    uint16_t *source;
    size_t destLen, sourceLen, n;

    if (argc < 2)
    {
        fprintf(stderr, "Input size missing!\n");
        return 1;
    }
    sz_param.options_mask = OPTIONS_MASK;
    sz_param.bits_per_pixel = 16;
    sz_param.pixels_per_block = PIXELS_PER_BLOCK;
    sz_param.pixels_per_scanline = PIXELS_PER_SCANLINE;

    sourceLen = destLen = atoi(argv[1]);

    source = (uint16_t *)malloc(sourceLen * sizeof(uint16_t));
    dest = (unsigned char *)malloc(destLen);

    if (source == NULL || dest == NULL)
        return 1;

    n = 0;
    while((c = getc(stdin)) != EOF)
    {
        source[n] = c;
        source[n] |= getc(stdin) << 8;
        n++;
    }
    sourceLen = n * sizeof(uint16_t);

    fprintf(stderr, "Uncompressed size is %li\n", sourceLen);

    status = SZ_BufftoBuffCompress(dest, &destLen, source, sourceLen, &sz_param);
    if (status != SZ_OK)
        return status;

    fprintf(stderr, "Compressed size is %li\n", destLen);

    status = SZ_BufftoBuffDecompress(source, &sourceLen, dest, destLen, &sz_param);
    if (status != SZ_OK)
        return status;

    fprintf(stderr, "Uncompressed size is %li again\n", sourceLen);

    for(c = 0; c < sourceLen / sizeof(uint16_t); c++)
    {
        putc(source[c], stdout);
        putc(source[c] >> 8, stdout);
    }
    free(source);
    free(dest);
    return 0;
}
