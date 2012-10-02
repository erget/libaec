#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "libaec.h"
#include "check_aec.h"

#define BUF_SIZE 1024 * 3

int check_block_sizes(struct test_state *state, int id, int id_len)
{
    int bs, status;

    for (bs = 8; bs <= 64; bs *= 2) {
        state->strm->block_size = bs;
        state->strm->rsi = state->buf_len
            / (bs * state->byte_per_sample);

        status = encode_decode(state);
        if (status)
            return status;

        if ((state->cbuf[0] >> (8 - id_len)) != id) {
            printf("FAIL: Unexpected block of size %i created %x.\n",
                   bs, state->cbuf[0] >> (8 - id_len));
            return 99;
        }
    }
    return 0;
}

int check_zero(struct test_state *state)
{
    int status;

    memset(state->ubuf, 0x55, state->buf_len);

    printf("Checking zero blocks ... ");
    status = check_block_sizes(state, 0, state->id_len + 1);
    if (status)
        return status;

    printf ("OK\n");
    return 0;
}

int check_splitting(struct test_state *state, int k)
{
    int status, size;
    unsigned char *tmp;

    size = state->byte_per_sample;

    for (tmp = state->ubuf;
         tmp < state->ubuf + state->buf_len;
         tmp += 4 * state->byte_per_sample) {
        state->out(tmp, state->xmin + (1ULL << (k - 1)) - 1, size);
        state->out(tmp + size, state->xmin, size);
        state->out(tmp + 2 * size, state->xmin + (1ULL << (k + 1)) - 1, size);
        state->out(tmp + 3 * size, state->xmin, size);
    }

    printf("Checking splitting with k=%i ... ", k);
    status = check_block_sizes(state, k + 1, state->id_len);
    if (status)
        return status;

    printf ("OK\n");
    return 0;
}

int check_uncompressed(struct test_state *state)
{
    int status, size;
    unsigned char *tmp;

    size = state->byte_per_sample;

    for (tmp = state->ubuf;
         tmp < state->ubuf + state->buf_len;
         tmp += 2 * state->byte_per_sample) {
        state->out(tmp, state->xmax, size);
        state->out(tmp + size, state->xmin, size);
    }

    printf("Checking uncompressed ... ");
    status = check_block_sizes(state,
                               (1ULL << state->id_len) - 1,
                               state->id_len);
    if (status)
        return status;

    printf ("OK\n");
    return 0;
}

int check_fs(struct test_state *state)
{
    int status, size;
    unsigned char *tmp;

    size = state->byte_per_sample;

    for (tmp = state->ubuf;
         tmp < state->ubuf + state->buf_len;
         tmp += 4 * state->byte_per_sample) {
        state->out(tmp, state->xmin + 2, size);
        state->out(tmp + size, state->xmin, size);
        state->out(tmp + 2 * size, state->xmin, size);
        state->out(tmp + 3 * size, state->xmin, size);
    }

    printf("Checking FS ... ");
    status = check_block_sizes(state, 1, state->id_len);
    if (status)
        return status;

    printf ("OK\n");
    return 0;
}

int check_se(struct test_state *state)
{
    int status, size;
    unsigned char *tmp;

    size = state->byte_per_sample;

    for (tmp = state->ubuf;
         tmp < state->ubuf + state->buf_len;
         tmp += 8 * size) {
        state->out(tmp, 0, size);
        state->out(tmp + size, 0, size);
        state->out(tmp + 2 * size, 0, size);
        state->out(tmp + 3 * size, 0, size);
        state->out(tmp + 4 * size, 0, size);
        state->out(tmp + 5 * size, 0, size);
        state->out(tmp + 6 * size, 0, size);
        state->out(tmp + 7 * size, 1, size);
    }

    printf("Checking Second Extension ... ");
    status = check_block_sizes(state, 1, state->id_len + 1);
    if (status)
        return status;

    printf ("OK\n");
    return 0;
}

int check_bps(struct test_state *state)
{
    int k, status, bps;

    for (bps = 8; bps <= 32; bps += 8) {
        state->strm->bit_per_sample = bps;
        if (bps == 24)
            state->strm->flags |= AEC_DATA_3BYTE;
        else
            state->strm->flags &= ~AEC_DATA_3BYTE;

        update_state(state);

        status = check_zero(state);
        if (status)
            return status;

        status = check_se(state);
        if (status)
            return status;

        status = check_uncompressed(state);
        if (status)
            return status;

        status = check_fs(state);
        if (status)
            return status;

        for (k = 1; k < bps - 2; k++) {
            status = check_splitting(state, k);
            if (status)
                return status;
        }
        printf("All checks with %i bit per sample passed.\n", bps);
    }
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

    printf("----------------------------\n");
    printf("Checking LSB first, unsigned\n");
    printf("----------------------------\n");
    status = check_bps(&state);
    if (status)
        goto DESTRUCT;

    printf("--------------------------\n");
    printf("Checking LSB first, signed\n");
    printf("--------------------------\n");
    strm.flags |= AEC_DATA_SIGNED;

    status = check_bps(&state);
    if (status)
        goto DESTRUCT;

    strm.flags &= ~AEC_DATA_SIGNED;
    strm.flags |= AEC_DATA_MSB;

    printf("----------------------------\n");
    printf("Checking MSB first, unsigned\n");
    printf("----------------------------\n");
    status = check_bps(&state);
    if (status)
        goto DESTRUCT;

    printf("--------------------------\n");
    printf("Checking MSB first, signed\n");
    printf("--------------------------\n");
    strm.flags |= AEC_DATA_SIGNED;

    status = check_bps(&state);
    if (status)
        goto DESTRUCT;

DESTRUCT:
    free(state.ubuf);
    free(state.cbuf);
    free(state.obuf);

    return status;
}
