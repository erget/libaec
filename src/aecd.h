#ifndef AELIB_H
#define AELIB_H

#include <inttypes.h>

struct internal_state;

typedef struct _ae_stream
{
    const uint8_t *next_in;
    size_t avail_in;  /* number of bytes available at next_in */
    size_t total_in;  /* total number of input bytes read so far */

    uint32_t *next_out;
    size_t avail_out; /* remaining free space at next_out */
    size_t total_out; /* total number of bytes output so far */

    uint32_t bit_per_sample; /* resolution in bits per sample (n = 1,..., 32) */
    uint32_t block_size; /* block size in samples (J = 8 or 16) */
    uint32_t segment_size; /* set of blocks between consecutive reference
                              samples */
    uint8_t pp; /* pre/post-processor used? */

    struct internal_state *state;
} ae_stream;

typedef ae_stream *ae_streamp;

#define AE_OK            0
#define AE_STREAM_END    1
#define AE_ERRNO        (-1)
#define AE_STREAM_ERROR (-2)
#define AE_DATA_ERROR   (-3)
#define AE_MEM_ERROR    (-4)

#define AE_NO_FLUSH      0
#define AE_PARTIAL_FLUSH 1
#define AE_SYNC_FLUSH    2
#define AE_FULL_FLUSH    3
#define AE_FINISH        4
#define AE_BLOCK         5
#define AE_TREES         6

int ae_decode_init(ae_streamp strm);

int ae_decode(ae_streamp strm, int flush);

#endif /* AELIB_H */
