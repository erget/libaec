#ifndef LIBAE_H
#define LIBAE_H

#include <inttypes.h>

struct internal_state;

typedef struct _ae_stream
{
    const uint8_t *next_in;
    size_t avail_in;         /* number of bytes available at
                              * next_in */
    size_t total_in;         /* total number of input bytes read so
                              * far */
    uint8_t *next_out;
    size_t avail_out;        /* remaining free space at next_out */
    size_t total_out;        /* total number of bytes output so far */
    uint32_t bit_per_sample; /* resolution in bits per sample (n =
                              * 1,..., 32) */
    uint32_t block_size;     /* block size in samples (J = 8 or 16) */
    uint32_t segment_size;   /* set of blocks between consecutive
                              * reference samples */
    uint32_t flags; 

    struct internal_state *state;
} ae_stream;

typedef ae_stream *ae_streamp;

/* Coder flags */
#define AE_DATA_UNSIGNED    0
#define AE_DATA_SIGNED      1
#define AE_DATA_LSB         8
#define AE_DATA_MSB        16
#define AE_DATA_PREPROCESS 32 /* Set if preprocessor should be used */


/* Return codes of library functions */
#define AE_OK            0
#define AE_STREAM_END    1
#define AE_ERRNO        (-1)
#define AE_STREAM_ERROR (-2)
#define AE_DATA_ERROR   (-3)
#define AE_MEM_ERROR    (-4)

/* Options for flushing */
#define AE_NO_FLUSH      0 /* Do not enforce output flushing. More
                            * input may be provided with later
                            * calls. So far only relevant for
                            * encoding. */
#define AE_FLUSH         1 /* Flush output and end encoding. The last
                            * call to ae_encode() must set AE_FLUSH to
                            * drain all output.
                            *
                            * It is not possible to continue encoding
                            * of the same stream after it has been
                            * flushed because the last byte may be
                            * padded with fill bits. */

int ae_decode_init(ae_streamp strm);
int ae_decode(ae_streamp strm, int flush);

int ae_encode_init(ae_streamp strm);
int ae_encode(ae_streamp strm, int flush);

#endif /* LIBAE_H */
