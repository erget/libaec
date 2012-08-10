#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include "libae.h"

#define CHUNK_OUT 0x4000
#define CHUNK_IN 1024
#define ALL_IN 9478

int main(int argc, char *argv[])
{
    ae_stream strm;
    uint8_t *in;
    uint8_t *out;
    int chunk_in, chunk_out, i, c, total_out, status;
    int input_avail, output_avail;

    if (argc == 3)
    {
        chunk_in = atoi(argv[1]);
        chunk_out = atoi(argv[2]);
    }
    else
    {
        chunk_in = CHUNK_IN;
        chunk_out = CHUNK_OUT;
    }

    out = (uint8_t *)malloc(chunk_out);
    in = (uint8_t *)malloc(chunk_in * sizeof(uint8_t));
    if (in == NULL || out == NULL)
        return 1;

    strm.bit_per_sample = 8;
    strm.block_size = 8;
    strm.segment_size = 2;
    strm.flags = AE_DATA_UNSIGNED | AE_DATA_PREPROCESS;

    if (ae_encode_init(&strm) != AE_OK)
        return 1;

    total_out = 0;
    strm.avail_in = 0;
    strm.avail_out = chunk_out;
    strm.next_out = out;

    input_avail = 1;
    output_avail = 1;

    while(input_avail || output_avail)
    {
        if (strm.avail_in == 0)
        {
            i = 0;
            while(i < chunk_in && (c = getc(stdin)) != EOF)
            {
                in[i] = c;
                /* in[i] |= getc(stdin) << 8; */
                /* in[i] |= getc(stdin) << 16; */
                /* in[i] |= getc(stdin) << 24; */
                i++;
            }
            strm.avail_in = i;

            strm.next_in = (uint8_t *)in;
            if (c == EOF)
                input_avail = 0;
        }

        if ((status = ae_encode(&strm, AE_NO_FLUSH)) != AE_OK)
        {
            fprintf(stderr, "error is %i\n", status);
            return 1;
        }

        if (strm.total_out - total_out > 0)
        {
            for (i=0; i < strm.total_out - total_out; i++)
            {
                putc(out[i], stdout);
            }
            total_out = strm.total_out;
            output_avail = 1;
            strm.next_out = out;
            strm.avail_out = chunk_out;
        }
        else
        {
            output_avail = 0;
        }

    }

    if ((status = ae_encode(&strm, AE_FLUSH)) != AE_OK)
    {
        fprintf(stderr, "error is %i\n", status);
        return 1;
    }

    if (strm.total_out - total_out > 0)
    {
        for (i=0; i < strm.total_out - total_out; i++)
        {
            putc(out[i], stdout);
        }
    }
    ae_encode_end(&strm);
    free(in);
    free(out);
    return 0;
}
