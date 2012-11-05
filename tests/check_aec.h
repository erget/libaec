#ifndef CHECK_AEC_H
#define CHECK_AEC_H
#include "libaec.h"

struct test_state {
    int (* codec)(struct test_state *state);
    int id_len;
    int byte_per_sample;
    unsigned char *ubuf;
    unsigned char *cbuf;
    unsigned char *obuf;
    size_t ibuf_len; /* input buffer legth may be shorter than buf_len */
    size_t buf_len;
    size_t cbuf_len;
    long long int xmax;
    long long int xmin;
    void (*out)(unsigned char *dest, unsigned int val, int size);
    struct aec_stream *strm;
};

int update_state(struct test_state *state);
int encode_decode_small(struct test_state *state);
int encode_decode_large(struct test_state *state);


#endif /* CHECK_AEC_H */
