#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include "libaec.h"

#define CHUNK 1024

int main(int argc, char *argv[])
{
    struct aec_stream strm;
    unsigned char *in;
    unsigned char *out;
    int chunk, total_out, status, c;
    int input_avail, output_avail;
    char *outfn, *infn, *ext;
    FILE *infp, *outfp;
    int cflag = 0;
    int dflag = 0;

    chunk = CHUNK;
    strm.bit_per_sample = 8;
    strm.block_size = 8;
    strm.rsi = 2;
    strm.flags = AEC_DATA_PREPROCESS;
    opterr = 0;

    while ((c = getopt (argc, argv, "d3Mscb:B:R:J:")) != -1)
        switch (c)
        {
        case 'd':
            dflag = 1;
            break;
        case 'b':
            chunk = atoi(optarg);
            break;
        case 'B':
            strm.bit_per_sample = atoi(optarg);
            break;
        case 'J':
            strm.block_size = atoi(optarg);
            break;
        case 'R':
            strm.rsi = atoi(optarg);
            break;
        case 'c':
            cflag = 1;
            break;
        case 's':
            strm.flags |= AEC_DATA_SIGNED;
            break;
        case 'M':
            strm.flags |= AEC_DATA_MSB;
            break;
        case '3':
            strm.flags |= AEC_DATA_3BYTE;
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

    if (strm.bit_per_sample > 16)
    {
        if (strm.bit_per_sample <= 24 && strm.flags & AEC_DATA_3BYTE)
            chunk *= 3;
        else
            chunk *= 4;
    }
    else if (strm.bit_per_sample > 8)
    {
        chunk *= 2;
    }

    out = (unsigned char *)malloc(chunk);
    in = (unsigned char *)malloc(chunk);


    if (in == NULL || out == NULL)
        exit(-1);

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

        if (dflag)
        {
            if ((ext = strstr(infn, ".aec")) == NULL)
            {
                fprintf(stderr, "Error: input file needs to end with .aec\n");
                exit(-1);
            }
            strncpy(outfn, infn, ext - infn);
        }
        else
        {
            sprintf(outfn, "%s.aec", infn);
        }

        if ((outfp = fopen(outfn, "w")) == NULL)
            exit(-1);
    }

    if (dflag)
    {
        if (aec_decode_init(&strm) != AEC_OK)
            return 1;
    }
    else
    {
        if (aec_encode_init(&strm) != AEC_OK)
            return 1;
    }

    while(input_avail || output_avail)
    {
        if (strm.avail_in == 0 && input_avail)
        {
            strm.avail_in = fread(in, 1, chunk, infp);
            if (strm.avail_in != chunk)
                input_avail = 0;
            strm.next_in = in;
        }

        if (dflag)
            status = aec_decode(&strm, AEC_NO_FLUSH);
        else
            status = aec_encode(&strm, AEC_NO_FLUSH);

        if (status != AEC_OK)
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

    if (dflag)
    {
        aec_decode_end(&strm);
    }
    else
    {
        if ((status = aec_encode(&strm, AEC_FLUSH)) != AEC_OK)
        {
            fprintf(stderr, "error is %i\n", status);
            return 1;
        }

        if (strm.total_out - total_out > 0)
        {
            fwrite(out, strm.total_out - total_out, 1, outfp);
        }

        aec_encode_end(&strm);
    }

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
