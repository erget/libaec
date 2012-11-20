#ifndef LIBAEC_H
#define LIBAEC_H

#include <stddef.h>

struct internal_state;

struct aec_stream {
    const unsigned char *next_in;
    size_t avail_in;            /* number of bytes available at
                                 * next_in
                                 */
    size_t total_in;            /* total number of input bytes read so
                                 * far
                                 */
    unsigned char *next_out;
    size_t avail_out;           /* remaining free space at next_out */
    size_t total_out;           /* total number of bytes output so far */
    int bits_per_sample;        /* resolution in bits per sample (n =
                                 * 1, ..., 32)
                                 */
    int block_size;             /* block size in samples */
    int rsi;                    /* Reference sample interval, the number
                                 * of _blocks_ between consecutive
                                 * reference samples (up to 4096).
                                 */
    int flags;

    struct internal_state *state;
};

/* Sample data description flags */
#define AEC_DATA_SIGNED 1        /* Samples are signed. Telling libaec
                                  * this results in a slightly better
                                  * compression ratio. Default is
                                  * unsigned.
                                  */
#define AEC_DATA_3BYTE 2         /* 24 bit samples are coded in 3 bytes */
#define AEC_DATA_MSB 4           /* Samples are stored with their most
                                  * significant bit first. This has
                                  * nothing to do with the endianness
                                  * of the host. Default is LSB.
                                  */
#define AEC_DATA_PREPROCESS 8    /* Set if preprocessor should be used */

/* Return codes of library functions */
#define AEC_OK            0
#define AEC_CONF_ERROR   (-1)
#define AEC_STREAM_ERROR (-2)
#define AEC_DATA_ERROR   (-3)
#define AEC_MEM_ERROR    (-4)

/* Options for flushing */
#define AEC_NO_FLUSH      0      /* Do not enforce output
                                  * flushing. More input may be
                                  * provided with later calls. So far
                                  * only relevant for encoding.
                                  */
#define AEC_FLUSH         1      /* Flush output and end encoding. The
                                  * last call to aec_encode() must set
                                  * AEC_FLUSH to drain all output.
                                  *
                                  * It is not possible to continue
                                  * encoding of the same stream after it
                                  * has been flushed because the last byte
                                  * may be padded with fill bits.
                                  */

/* Streaming encoding and decoding functions */
int aec_encode_init(struct aec_stream *strm);
int aec_encode(struct aec_stream *strm, int flush);
int aec_encode_end(struct aec_stream *strm);

int aec_decode_init(struct aec_stream *strm);
int aec_decode(struct aec_stream *strm, int flush);
int aec_decode_end(struct aec_stream *strm);

/* Utility functions for encoding or decoding a memory buffer. */
int aec_buffer_encode(struct aec_stream *strm);
int aec_buffer_decode(struct aec_stream *strm);

#endif /* LIBAEC_H */
