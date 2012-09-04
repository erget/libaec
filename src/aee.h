#ifndef AAE_H
#define AAE_H

#include <inttypes.h>
#include "libae.h"

#define M_CONTINUE 1
#define M_EXIT 0

typedef struct internal_state {
    int (*mode)(ae_streamp);
    void (*get_block)(ae_streamp);
    int64_t (*get_sample)(ae_streamp);

    int id_len;             /* bit length of code option identification key */
    int64_t xmin;           /* minimum integer for preprocessing */
    int64_t xmax;           /* maximum integer for preprocessing */
    int i;                  /* counter */
    int64_t *block_buf;     /* RSI blocks of input */
    int blocks_avail;       /* remaining blocks in buffer */
    int64_t *block_p;       /* pointer to current block */
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
} encode_state;

#endif
