/**
 * @file decode.c
 *
 * @author Mathis Rosenhauer, Deutsches Klimarechenzentrum
 * @author Moritz Hanke, Deutsches Klimarechenzentrum
 * @author Joerg Behrens, Deutsches Klimarechenzentrum
 * @author Luis Kornblueh, Max-Planck-Institut fuer Meteorologie
 *
 * @section LICENSE
 * Copyright 2012
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
 * Adaptive Entropy Decoder
 * Based on CCSDS documents 121.0-B-2 and 120.0-G-2
 *
 */

#ifndef DECODE_H
#define DECODE_H

#include <config.h>

#if HAVE_STDINT_H
# include <stdint.h>
#endif

#include "libaec.h"

#define M_CONTINUE 1
#define M_EXIT 0

#define MIN(a, b) (((a) < (b))? (a): (b))

struct internal_state {
    int (*mode)(struct aec_stream *);
    int id;            /* option ID */
    int id_len;        /* bit length of code option identification key */
    int (**id_table)(struct aec_stream *); /* table maps IDs to states */
    void (*flush_output)(struct aec_stream *);
    int ref_int;       /* reference sample is every ref_int samples */
    int64_t last_out;  /* previous output for post-processing */
    int64_t xmin;      /* minimum integer for post-processing */
    int64_t xmax;      /* maximum integer for post-processing */
    int in_blklen;     /* length of uncompressed input block
                          should be the longest possible block */
    int out_blklen;    /* length of output block in bytes */
    int n, i;          /* counter for samples */
    uint32_t *block;   /* block buffer for split-sample options */
    int se;            /* set if second extension option is selected */
    uint64_t acc;      /* accumulator for currently used bit sequence */
    int bitp;          /* bit pointer to the next unused bit in accumulator */
    int fs;            /* last fundamental sequence in accumulator */
    int ref;           /* 1 if current block has reference sample */
    int pp;            /* 1 if postprocessor has to be used */
    int bytes_per_sample;
    int *se_table;
    uint32_t *buf;
    uint32_t *bufp;
    uint32_t buf_size;
    uint32_t *flush_start;
} decode_state;

#endif /* DECODE_H */
