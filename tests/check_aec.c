#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "libaec.h"
#include "check_aec.h"

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

int update_state(struct test_state *state)
{
    struct aec_stream *strm = state->strm;

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

int encode_decode(struct test_state *state)
{
    int status, i, to;
    struct aec_stream *strm = state->strm;

    strm->avail_in = state->ibuf_len;
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

    if (memcmp(state->ubuf, state->obuf, state->ibuf_len)) {
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
