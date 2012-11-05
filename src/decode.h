#ifndef DECODE_H
#define DECODE_H

#include <config.h>

#if HAVE_STDINT_H
# include <stdint.h>
#endif

#include "libaec.h"

#define M_CONTINUE 1
#define M_EXIT 0

#define SAFE(strm) (strm->avail_in >= strm->state->in_blklen    \
              && strm->avail_out >= strm->state->out_blklen)

#define ROS 5
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
    int64_t *block;    /* block buffer for split-sample options */
    int se;            /* set if second extension option is selected */
    uint64_t acc;      /* accumulator for currently used bit sequence */
    int bitp;          /* bit pointer to the next unused bit in accumulator */
    int fs;            /* last fundamental sequence in accumulator */
    int ref;           /* 1 if current block has reference sample */
    int pp;            /* 1 if postprocessor has to be used */
    int byte_per_sample;
    int *se_table;
    uint32_t *buf_out;
    uint32_t buf_next_out;
    uint32_t buf_size;
    uint32_t flush_start;
    uint32_t flush_end;
} decode_state;

#endif /* DECODE_H */
