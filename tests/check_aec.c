#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include "libaec.h"

#define BUF_SIZE 1024 * 3

struct test_state {
    int id_len;
    int byte_per_sample;
    uint8_t *ubuf;
    uint8_t *cbuf;
    uint8_t *obuf;
    size_t buf_len;
    size_t cbuf_len;
    void (*out)(uint8_t *dest, uint32_t val, int size);
};

static void out_lsb(uint8_t *dest, uint32_t val, int size)
{
    int i;

    for (i = 0; i < size; i++)
        dest[i] = val >> (8 * i);
}

static void out_msb(uint8_t *dest, uint32_t val, int size)
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

int check_zero(struct aec_stream *strm, struct test_state *state)
{
    int bs, status;

    printf("Checking zero blocks with bit per sample %i ... ",
           strm->bit_per_sample);

    for (bs = 8; bs <= 64; bs *= 2) {
        memset(state->ubuf, 0x55, state->buf_len);
        strm->block_size = bs;
        strm->rsi = state->buf_len / (bs * state->byte_per_sample);

        status = encode_decode(strm, state);
        if (status)
            return status;

        if ((state->cbuf[0] >> (8 - state->id_len)) != 0) {
            printf("FAIL: Unexpected block created.\n");
            return 99;
        }
    }
    printf ("OK\n");
    return 0;
}

int check_splitting(struct aec_stream *strm, struct test_state *state, int k)
{
    int bs, status;
    uint8_t *tmp;
    size_t size;

    size = state->byte_per_sample;

    for (tmp = state->ubuf;
         tmp < state->ubuf + state->buf_len;
         tmp += 4 * state->byte_per_sample) {
        state->out(tmp, (1ULL << (k - 1)) - 1, size);
        state->out(tmp + size, 0, size);
        state->out(tmp + 2 * size, (1ULL << (k + 1)) - 1, size);
        state->out(tmp + 3 * size, 0, size);
    }

    printf("Checking splitting with k=%i, bit per sample %i ... ",
           k, strm->bit_per_sample);

    for (bs = 8; bs <= 64; bs *= 2) {
        strm->block_size = bs;
        strm->rsi = state->buf_len / (bs * state->byte_per_sample);

        status = encode_decode(strm, state);
        if (status)
            return status;

        if ((state->cbuf[0] >> (8 - state->id_len)) != k + 1) {
            printf("FAIL: Unexpected block of size %i created %i.\n",
                   bs, state->cbuf[0] >> 5);
            return 99;
        }
    }
    printf ("OK\n");
    return 0;
}

int main (void)
{
    int k, status, bps;
    struct aec_stream strm;
    struct test_state state;

    state.buf_len = BUF_SIZE;
    state.cbuf_len = 2 * BUF_SIZE;

    state.ubuf = (uint8_t *)malloc(state.buf_len);
    state.cbuf = (uint8_t *)malloc(state.cbuf_len);
    state.obuf = (uint8_t *)malloc(state.buf_len);

    if (!state.ubuf || !state.cbuf || !state.obuf) {
        printf("Not enough memory.\n");
        return 99;
    }

    for (bps = 8; bps <= 32; bps += 8) {
        strm.bit_per_sample = bps;
        strm.flags = AEC_DATA_PREPROCESS;
        if (bps == 24)
            strm.flags |= AEC_DATA_3BYTE;

        update_state(&strm, &state);

        status = check_zero(&strm, &state);
        if (status)
            return status;

        for (k = 1; k < bps - 2; k++) {
            status = check_splitting(&strm, &state, k);
            if (status)
                return status;
        }
    }

    free(state.ubuf);
    free(state.cbuf);
    free(state.obuf);

    return 0;
}
