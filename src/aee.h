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
    int64_t last_in;        /* previous input for preprocessing */
    int64_t xmin;           /* minimum integer for preprocessing */
    int64_t xmax;           /* maximum integer for preprocessing */
    int i;                  /* counter for samples */
    int64_t *in_block;      /* input block buffer */
    int in_blklen;          /* input block length in byte */
    int64_t in_total_blocks;/* total blocks in */
    uint8_t *out_block;     /* output block buffer */
    int out_blklen;         /* output block length in byte */
    uint8_t *out_bp;        /* pointer to current output */
    int out_direct;         /* output to strm->next_out (1)
                               or out_block (0) */
    int bitp;               /* bit pointer to the next unused bit in accumulator */
    int block_deferred;     /* there is a block in the input buffer
                               but we first have to emit a zero block */
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
