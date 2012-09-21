#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "libaec.h"

#define BUF_SIZE 1024 * 3

struct test_state {
    int id_len;
    int byte_per_sample;
    unsigned char *ubuf;
    unsigned char *cbuf;
    unsigned char *obuf;
    size_t buf_len;
    size_t cbuf_len;
    long long int xmax;
    long long int xmin;
    void (*out)(unsigned char *dest, unsigned int val, int size);
};

static void out_lsb(unsigned char *dest, unsigned int val, int size)
{
    int i;

    for (i = 0; i < size; i++)
        dest[i] = val >> (8 * i);
}

static void out_msb(unsigned char *dest, unsigned int val, int size)
{
    int i;

    for (i = 0; i < size; i++)
        dest[i] = val >> (8 * (size - 1 - i));
}

static int update_state(struct aec_stream *strm, struct test_state *state)
{
    if (strm->bit_per_sample > 16) {
        state->id_len = 5;

        if (strm->bit_per_sample <= 24 && strm->flags & AEC_DATA_3BYTE) {
            state->byte_per_sample = 3;
        } else {
            state->byte_per_sample = 4;
        }
    }
    else if (strm->bit_per_sample > 8) {
        state->id_len = 4;
        state->byte_per_sample = 2;
    } else {
        state->id_len = 3;
        state->byte_per_sample = 1;
    }

    if (strm->flags & AEC_DATA_MSB)
        state->out = out_msb;
    else
        state->out = out_lsb;

    if (strm->flags & AEC_DATA_SIGNED) {
        state->xmin = -(1ULL << (strm->bit_per_sample - 1));
        state->xmax = (1ULL << (strm->bit_per_sample - 1)) - 1;
    } else {
        state->xmin = 0;
        state->xmax = (1ULL << strm->bit_per_sample) - 1;
    }

    return 0;
}

int encode_decode(struct aec_stream *strm, struct test_state *state)
{
    int status, i, to;

    strm->avail_in = state->buf_len;
    strm->avail_out = state->cbuf_len;
    strm->next_in = state->ubuf;
    strm->next_out = state->cbuf;

    status = aec_encode_init(strm);
    if (status != AEC_OK) {
        printf("Init failed.\n");
        return 99;
    }

    status = aec_encode(strm, AEC_FLUSH);
    if (status != AEC_OK) {
        printf("Encode failed.\n");
        return 99;
    }

    aec_encode_end(strm);

    strm->avail_in = strm->total_out;
    strm->avail_out = state->buf_len;
    strm->next_in = state->cbuf;
    strm->next_out = state->obuf;
    to = strm->total_out;

    status = aec_decode_init(strm);
    if (status != AEC_OK) {
        printf("Init failed.\n");
        return 99;
    }

    status = aec_decode(strm, AEC_FLUSH);
    if (status != AEC_OK) {
        printf("Decode failed.\n");
        return 99;
    }

    if (memcmp(state->ubuf, state->obuf, state->buf_len)) {
        printf("FAIL: Uncompressed output differs from input.\n");

        printf("\nuncompressed buf");
        for (i = 0; i < 80; i++) {
            if (i % 8 == 0)
                printf("\n");
            printf("%02x ", state->ubuf[i]);
        }
        printf("\n\ncompressed buf len %i", to);
        for (i = 0; i < 80; i++) {
            if (i % 8 == 0)
                printf("\n");
            printf("%02x ", state->cbuf[i]);
        }
        printf("\n\ndecompressed buf");
        for (i = 0; i < 80; i++) {
            if (i % 8 == 0)
                printf("\n");
            printf("%02x ", state->obuf[i]);
        }
        printf("\n");
        return 99;
    }
    aec_decode_end(strm);
    return 0;
}

int check_block_sizes(struct aec_stream *strm,
                      struct test_state *state,
                      int id)
{
    int bs, status;

    for (bs = 8; bs <= 64; bs *= 2) {
        strm->block_size = bs;
        strm->rsi = state->buf_len / (bs * state->byte_per_sample);

        status = encode_decode(strm, state);
        if (status)
            return status;

        if ((state->cbuf[0] >> (8 - state->id_len)) != id) {
            printf("FAIL: Unexpected block of size %i created %x.\n",
                   bs, state->cbuf[0] >> (8 - state->id_len));
            return 99;
        }
    }
    return 0;
}

int check_zero(struct aec_stream *strm, struct test_state *state)
{
    int status;

    memset(state->ubuf, 0x55, state->buf_len);

    printf("Checking zero blocks ... ");
    status = check_block_sizes(strm, state, 0);
    if (status)
        return status;

    printf ("OK\n");
    return 0;
}

int check_splitting(struct aec_stream *strm, struct test_state *state, int k)
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
    status = check_block_sizes(strm, state, k + 1);
    if (status)
        return status;

    printf ("OK\n");
    return 0;
}

int check_uncompressed(struct aec_stream *strm, struct test_state *state)
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
    status = check_block_sizes(strm, state, (1ULL << state->id_len) - 1);
    if (status)
        return status;

    printf ("OK\n");
    return 0;
}

int check_fs(struct aec_stream *strm, struct test_state *state)
{
    int status, size;
    unsigned char *tmp;

    size = state->byte_per_sample;

    for (tmp = state->ubuf;
         tmp < state->ubuf + state->buf_len;
         tmp += 2 * state->byte_per_sample) {
        state->out(tmp, state->xmin + 1, size);
        state->out(tmp + size, state->xmin, size);
    }

    printf("Checking FS ... ");
    status = check_block_sizes(strm, state, 1);
    if (status)
        return status;

    printf ("OK\n");
    return 0;
}

int check_se(struct aec_stream *strm, struct test_state *state)
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
    status = check_block_sizes(strm, state, 0);
    if (status)
        return status;

    printf ("OK\n");
    return 0;
}

int check_bps(struct aec_stream *strm, struct test_state *state)
{
    int k, status, bps;

    for (bps = 8; bps <= 32; bps += 8) {
        strm->bit_per_sample = bps;
        if (bps == 24)
            strm->flags |= AEC_DATA_3BYTE;
        else
            strm->flags &= ~AEC_DATA_3BYTE;

        update_state(strm, state);

        status = check_zero(strm, state);
        if (status)
            return status;

        status = check_se(strm, state);
        if (status)
            return status;

        status = check_uncompressed(strm, state);
        if (status)
            return status;

        status = check_fs(strm, state);
        if (status)
            return status;

        for (k = 1; k < bps - 2; k++) {
            status = check_splitting(strm, state, k);
            if (status)
                return status;
        }
        printf("All checks with %i bit per sample passed.\n\n", bps);
    }
    return 0;
}

int main (void)
{
    int status;
    struct aec_stream strm;
    struct test_state state;

    state.buf_len = BUF_SIZE;
    state.cbuf_len = 2 * BUF_SIZE;

    state.ubuf = (unsigned char *)malloc(state.buf_len);
    state.cbuf = (unsigned char *)malloc(state.cbuf_len);
    state.obuf = (unsigned char *)malloc(state.buf_len);

    if (!state.ubuf || !state.cbuf || !state.obuf) {
        printf("Not enough memory.\n");
        return 99;
    }

    strm.flags = AEC_DATA_PREPROCESS;

    printf("----------------------------\n");
    printf("Checking LSB first, unsigned\n");
    printf("----------------------------\n");
    status = check_bps(&strm, &state);
    if (status)
        goto destruct;

    printf("--------------------------\n");
    printf("Checking LSB first, signed\n");
    printf("--------------------------\n");
    strm.flags |= AEC_DATA_SIGNED;

    status = check_bps(&strm, &state);
    if (status)
        goto destruct;

    strm.flags &= ~AEC_DATA_SIGNED;
    strm.flags |= AEC_DATA_MSB;

    printf("----------------------------\n");
    printf("Checking MSB first, unsigned\n");
    printf("----------------------------\n");
    status = check_bps(&strm, &state);
    if (status)
        goto destruct;

    printf("--------------------------\n");
    printf("Checking MSB first, signed\n");
    printf("--------------------------\n");
    strm.flags |= AEC_DATA_SIGNED;

    status = check_bps(&strm, &state);
    if (status)
        goto destruct;

destruct:
    free(state.ubuf);
    free(state.cbuf);
    free(state.obuf);

    return status;
}
