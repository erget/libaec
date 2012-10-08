#ifndef ENCODE_H
#define ENCODE_H

#include <config.h>

#if HAVE_STDINT_H
# include <stdint.h>
#endif

#include "libaec.h"

#define M_CONTINUE 1
#define M_EXIT 0

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
    int block_len;          /* input block length in byte */
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
    int64_t zero_ref_sample;/* reference sample of zero block */
    int zero_blocks;        /* number of contiguous zero blocks */
    int k;                  /* splitting position */
    int kmax;               /* maximum number for k depending on id_len */
    int flush;              /* flush option copied from argument */
};

#endif /* ENCODE_H */
