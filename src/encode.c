#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include "libae.h"

#define CHUNK 1024

int main(int argc, char *argv[])
{
    ae_stream strm;
    uint8_t *in;
    uint8_t *out;
    int chunk, total_out, status, c;
    int input_avail, output_avail;
    char *outfn, *infn;
    FILE *infp, *outfp;
    int cflag = 0;

    chunk = CHUNK;
    strm.bit_per_sample = 8;
    strm.block_size = 8;
    strm.rsi = 2;
    strm.flags = AE_DATA_MSB | AE_DATA_PREPROCESS;
    opterr = 0;

    while ((c = getopt (argc, argv, "cb:B:R:")) != -1)
        switch (c)
        {
        case 'b':
            chunk = 2 * atoi(optarg);
            break;
        case 'B':
            strm.bit_per_sample = atoi(optarg);
            break;
        case 'R':
            strm.rsi = atoi(optarg);
            break;
        case 'c':
            cflag = 1;
            break;
        case '?':
            if (optopt == 'b')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf (stderr,
                         "Unknown option character `\\x%x'.\n",
                         optopt);
            return 1;
        default:
            abort ();
        }

    if (optind < argc)
    {
        infn = argv[optind];
    }
    else
    {
        fprintf(stderr, "Usage: %s [ -c ] [ -b chunksize ] name\n", argv[0]);
        exit(-1);
    }

    out = (uint8_t *)malloc(chunk);
    in = (uint8_t *)malloc(chunk);


    if (in == NULL || out == NULL)
        exit(-1);

    if (ae_encode_init(&strm) != AE_OK)
        return 1;

    total_out = 0;
    strm.avail_in = 0;
    strm.avail_out = chunk;
    strm.next_out = out;

    input_avail = 1;
    output_avail = 1;

    if ((infp = fopen(infn, "r")) == NULL)
        exit(-1);

    if (cflag)
    {
        outfp = stdout;
    }
    else
    {
        outfn = malloc(strlen(infn) + 4);
        if (outfn == NULL)
            exit(-1);

        sprintf(outfn, "%s.aee", infn);

        if ((outfp = fopen(outfn, "w")) == NULL)
            exit(-1);
    }

    while(input_avail || output_avail)
    {
        if (strm.avail_in == 0 && input_avail)
        {
            strm.avail_in = fread(in, 1, chunk, infp);
            if (strm.avail_in != chunk)
                input_avail = 0;
            strm.next_in = (uint8_t *)in;
        }

        if ((status = ae_encode(&strm, AE_NO_FLUSH)) != AE_OK)
        {
            fprintf(stderr, "error is %i\n", status);
            return 1;
        }

        if (strm.total_out - total_out > 0)
        {
            fwrite(out, strm.total_out - total_out, 1, outfp);
            total_out = strm.total_out;
            output_avail = 1;
            strm.next_out = out;
            strm.avail_out = chunk;
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
        fwrite(out, strm.total_out - total_out, 1, outfp);
    }

    ae_encode_end(&strm);
    fclose(infp);
    fclose(outfp);
    free(in);
    free(out);
    if (!cflag)
    {
        unlink(infn);
        free(outfn);
    }
    return 0;
}
