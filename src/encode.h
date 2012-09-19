#ifndef ENCODE_H
#define ENCODE_H

#include <inttypes.h>
#include "libaec.h"

#define M_CONTINUE 1
#define M_EXIT 0

struct internal_state {
    int (*mode)(struct aec_stream *);
    void (*get_block)(struct aec_stream *);
    uint32_t (*get_sample)(struct aec_stream *);
    void (*preprocess)(struct aec_stream *);

    int id_len;             /* bit length of code option identification key */
    int64_t xmin;           /* minimum integer for preprocessing */
    int64_t xmax;           /* maximum integer for preprocessing */
    int i;                  /* counter */
    uint32_t *block_buf;     /* RSI blocks of input */
    int blocks_avail;       /* remaining blocks in buffer */
    uint32_t *block_p;       /* pointer to current block */
    int block_len;          /* input block length in byte */
    uint8_t *cds_buf;       /* Buffer for one Coded Data Set */
    int cds_len;            /* max cds length in byte */
    uint8_t *cds_p;         /* pointer to current output */
    int direct_out;         /* output to strm->next_out (1)
                               or cds_buf (0) */
    int bit_p;              /* bit pointer to the next unused bit in accumulator */
    int ref;                /* length of reference sample in current block
                               i.e. 0 or 1 depending on whether the block has
                               a reference sample or not */
    int zero_ref;           /* current zero block has a reference sample */
    int64_t zero_ref_sample;/* reference sample of zero block */
    int zero_blocks;        /* number of contiguous zero blocks */
    int k;                  /* splitting position */
    int flush;              /* flush option copied from argument */
};

#endif /* ENCODE_H */
