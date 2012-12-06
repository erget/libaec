/**
 * @file encode.h
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
 * Adaptive Entropy Encoder
 * Based on CCSDS documents 121.0-B-2 and 120.0-G-2
 *
 */

#ifndef ENCODE_H
#define ENCODE_H

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
    uint32_t (*get_sample)(struct aec_stream *);
    void (*get_rsi)(struct aec_stream *);
    void (*preprocess)(struct aec_stream *);

    int id_len;             /* bit length of code option identification key */
    int64_t xmin;           /* minimum integer for preprocessing */
    int64_t xmax;           /* maximum integer for preprocessing */
    int i;                  /* counter */
    uint32_t *data_pp;      /* RSI blocks of preprocessed input */
    uint32_t *data_raw;     /* RSI blocks of input */
    int blocks_avail;       /* remaining blocks in buffer */
    uint32_t *block;        /* current (preprocessed) input block */
    int rsi_len;            /* reference sample interval in byte */
    uint8_t *cds;           /* current Coded Data Set output */
    uint8_t *cds_buf;       /* buffer for one CDS (only used if
                             * strm->next_out cannot hold full CDS) */
    int cds_len;            /* max cds length in byte */
    int direct_out;         /* cds points to strm->next_out (1)
                             * or cds_buf (0) */
    int bits;               /* Free bits (LSB) in output buffer or
                             * accumulator */
    int ref;                /* length of reference sample in current
                             * block i.e. 0 or 1 depending on whether
                             * the block has a reference sample or
                             * not */
    int zero_ref;           /* current zero block has a reference sample */
    uint32_t zero_ref_sample;/* reference sample of zero block */
    int zero_blocks;        /* number of contiguous zero blocks */
    int block_nonzero;      /* 1 if this is the first non-zero block
                             * after one or more zero blocks */
    int k;                  /* splitting position */
    int kmax;               /* maximum number for k depending on id_len */
    int flush;              /* flush option copied from argument */
};

#endif /* ENCODE_H */
