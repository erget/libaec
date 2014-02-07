#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "libaec.h"
#include "check_aec.h"

#define BUF_SIZE (4 * 64 * 4)

int check_long_fs(struct test_state *state)
{
    int status, size;
    unsigned char *tmp;

    size = state->bytes_per_sample;

    for (tmp = state->ubuf;
         tmp < state->ubuf + state->buf_len;
         tmp += 2 * size) {
        state->out(tmp, state->xmin, size);
        state->out(tmp + size, state->xmin + 2, size);
    }

    state->out(state->ubuf + (64 + 1) * size, state->xmax-1, size);

    printf("Checking long fs ... ");

    status = state->codec(state);
    if (status)
        return status;

    printf ("%s\n", CHECK_PASS);
    return 0;
}

int main (void)
{
    int status;
    struct aec_stream strm;
    struct test_state state;

    state.buf_len = state.ibuf_len = BUF_SIZE;
    state.cbuf_len = 2 * BUF_SIZE;

    state.ubuf = (unsigned char *)malloc(state.buf_len);
    state.cbuf = (unsigned char *)malloc(state.cbuf_len);
    state.obuf = (unsigned char *)malloc(state.buf_len);

    if (!state.ubuf || !state.cbuf || !state.obuf) {
        printf("Not enough memory.\n");
        return 99;
    }

    strm.flags = AEC_DATA_PREPROCESS;
    state.strm = &strm;
    strm.bits_per_sample = 32;
    strm.block_size = 64;
    strm.rsi = 64;
    state.codec = encode_decode_large;
    update_state(&state);

    status = check_long_fs(&state);
    if (status)
        goto DESTRUCT;

DESTRUCT:
    free(state.ubuf);
    free(state.cbuf);
    free(state.obuf);

    return status;
}
