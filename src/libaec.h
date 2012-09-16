#ifndef LIBAEC_H
#define LIBAEC_H

#include <inttypes.h>
#include <stddef.h>

struct internal_state;

typedef struct _aec_stream
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
    uint32_t rsi;            /* Reference sample interval, the number of
                                blocks between consecutive reference
                                samples. */
    uint32_t flags;

    struct internal_state *state;
} aec_stream;

typedef aec_stream *aec_streamp;

/* Coder flags */
#define AEC_DATA_UNSIGNED     0
#define AEC_DATA_SIGNED       1
#define AEC_DATA_3BYTE        2  /* 24 bit samples coded in 3 bytes */
#define AEC_DATA_LSB          0
#define AEC_DATA_MSB         16
#define AEC_DATA_PREPROCESS  32  /* Set if preprocessor should be used */

/* Return codes of library functions */
#define AEC_OK            0
#define AEC_CONF_ERROR   (-1)
#define AEC_STREAM_ERROR (-2)
#define AEC_DATA_ERROR   (-3)
#define AEC_MEM_ERROR    (-4)

/* Options for flushing */
#define AEC_NO_FLUSH      0 /* Do not enforce output flushing. More
                            * input may be provided with later
                            * calls. So far only relevant for
                            * encoding.
                            */
#define AEC_FLUSH         1 /* Flush output and end encoding. The last
                            * call to aec_encode() must set AEC_FLUSH to
                            * drain all output.
                            *
                            * It is not possible to continue encoding
                            * of the same stream after it has been
                            * flushed because the last byte may be
                            * padded with fill bits.
                            */

int aec_decode_init(aec_streamp strm);
int aec_decode(aec_streamp strm, int flush);
int aec_decode_end(aec_streamp strm);

int aec_encode_init(aec_streamp strm);
int aec_encode(aec_streamp strm, int flush);
int aec_encode_end(aec_streamp strm);

#endif /* LIBAEC_H */
