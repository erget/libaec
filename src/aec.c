/**
 * @file aec.c
 *
 * @author Mathis Rosenhauer, Deutsches Klimarechenzentrum
 * @author Moritz Hanke, Deutsches Klimarechenzentrum
 * @author Joerg Behrens, Deutsches Klimarechenzentrum
 * @author Luis Kornblueh, Max-Planck-Institut fuer Meteorologie
 *
 * @section LICENSE
 * Copyright 2012 - 2014
 *
 * Mathis Rosenhauer,                 Luis Kornblueh
 * Moritz Hanke,
 * Joerg Behrens
 *
 * Deutsches Klimarechenzentrum GmbH  Max-Planck-Institut fuer Meteorologie
 * Bundesstr. 45a                     Bundesstr. 53
 * 20146 Hamburg                      20146 Hamburg
 * Germany                            Germany
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @section DESCRIPTION
 *
 * CLI frontend for Adaptive Entropy Coding library
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifndef _AIX
#include <getopt.h>
#endif

#include <libaec.h>

#define CHUNK 1024

int main(int argc, char *argv[])
{
    struct aec_stream strm;
    unsigned char *in;
    unsigned char *out;
    size_t total_out;
    int chunk, status, c;
    int input_avail, output_avail;
    char *outfn, *infn, *ext;
    FILE *infp, *outfp;
    int cflag = 0;
    int dflag = 0;

    chunk = CHUNK;
    strm.bits_per_sample = 8;
    strm.block_size = 8;
    strm.rsi = 2;
    strm.flags = AEC_DATA_PREPROCESS;
    opterr = 0;

    while ((c = getopt (argc, argv, "3b:cdj:mn:pr:st")) != -1)
        switch (c) {
        case '3':
            strm.flags |= AEC_DATA_3BYTE;
            break;
        case 'b':
            chunk = atoi(optarg);
            break;
        case 'c':
            cflag = 1;
            break;
        case 'd':
            dflag = 1;
            break;
        case 'j':
            strm.block_size = atoi(optarg);
            break;
        case 'm':
            strm.flags |= AEC_DATA_MSB;
            break;
        case 'n':
            strm.bits_per_sample = atoi(optarg);
            break;
        case 'p':
            strm.flags |= AEC_PAD_RSI;
            break;
        case 'r':
            strm.rsi = atoi(optarg);
            break;
        case 's':
            strm.flags |= AEC_DATA_SIGNED;
            break;
        case 't':
            strm.flags |= AEC_RESTRICTED;
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

    if (optind < argc) {
        infn = argv[optind];
    } else {
        fprintf(stderr, "Usage: %s [OPTION] SOURCE\n", argv[0]);
        fprintf(stderr, "\nOPTIONS\n");
        fprintf(stderr, "-3\n   24 bit samples are stored in 3 bytes\n");
        fprintf(stderr, "-b size\n   internal buffer size in bytes\n");
        fprintf(stderr, "-c\n   write output on standard output\n");
        fprintf(stderr, "-d\n   decode SOURCE. If -d is not used: encode.\n");
        fprintf(stderr, "-j samples\n   block size in samples\n");
        fprintf(stderr, "-m\n   samples are MSB first. Default is LSB\n");
        fprintf(stderr, "-n bits\n   bits per sample\n");
        fprintf(stderr, "-p\n   pad RSI to byte boundary\n");
        fprintf(stderr, "-r blocks\n   reference sample interval in blocks\n");
        fprintf(stderr, "-s\n   samples are signed. Default is unsigned\n");
        fprintf(stderr, "-t\n   use restricted set of code options\n\n");
        exit(-1);
    }

    if (strm.bits_per_sample > 16) {
        if (strm.bits_per_sample <= 24 && strm.flags & AEC_DATA_3BYTE)
            chunk *= 3;
        else
            chunk *= 4;
    } else if (strm.bits_per_sample > 8) {
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

    if (cflag) {
        outfp = stdout;
    } else {
        outfn = malloc(strlen(infn) + 4);
        if (outfn == NULL)
            exit(-1);

        if (dflag) {
            if ((ext = strstr(infn, ".rz")) == NULL) {
                fprintf(stderr, "ERROR: input file needs to end with .rz\n");
                exit(-1);
            }
            strncpy(outfn, infn, ext - infn);
        } else {
            sprintf(outfn, "%s.rz", infn);
        }

        if ((outfp = fopen(outfn, "w")) == NULL)
            exit(-1);
    }

    if (dflag) {
        if (aec_decode_init(&strm) != AEC_OK) {
            fprintf(stderr, "ERROR: Initialization failed\n");
            return 1;
        }
    } else {
        if (aec_encode_init(&strm) != AEC_OK) {
            fprintf(stderr, "ERROR: Initialization failed\n");
            return 1;
        }
    }

    while(input_avail || output_avail) {
        if (strm.avail_in == 0 && input_avail) {
            strm.avail_in = fread(in, 1, chunk, infp);
            if (strm.avail_in != chunk)
                input_avail = 0;
            strm.next_in = in;
        }

        if (dflag)
            status = aec_decode(&strm, AEC_NO_FLUSH);
        else
            status = aec_encode(&strm, AEC_NO_FLUSH);

        if (status != AEC_OK) {
            fprintf(stderr, "ERROR: %i\n", status);
            return 1;
        }

        if (strm.total_out - total_out > 0) {
            fwrite(out, strm.total_out - total_out, 1, outfp);
            total_out = strm.total_out;
            output_avail = 1;
            strm.next_out = out;
            strm.avail_out = chunk;
        } else {
            output_avail = 0;
        }

    }

    if (dflag) {
        aec_decode_end(&strm);
    } else {
        if ((status = aec_encode(&strm, AEC_FLUSH)) != AEC_OK) {
            fprintf(stderr, "ERROR: %i\n", status);
            return 1;
        }

        if (strm.total_out - total_out > 0)
            fwrite(out, strm.total_out - total_out, 1, outfp);

        aec_encode_end(&strm);
    }

    fclose(infp);
    fclose(outfp);
    free(in);
    free(out);
    if (!cflag) {
        unlink(infn);
        free(outfn);
    }
    return 0;
}
