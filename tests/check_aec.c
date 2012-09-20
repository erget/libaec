#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include "libaec.h"

#define BUF_SIZE 1024

int encode_decode(struct aec_stream *strm,
                  uint8_t *ubuf,
                  uint8_t *cbuf,
                  uint8_t *obuf,
                  size_t n)
{
    int status;

    strm->avail_in = n;
    strm->avail_out = n;
    strm->next_in = ubuf;
    strm->next_out = cbuf;

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
    strm->avail_out = n;
    strm->next_in = cbuf;
    strm->next_out = obuf;

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

    if (memcmp(ubuf, obuf, n)) {
        printf("FAIL: Uncompressed output differs from input.\n");
        return 99;
    }
    aec_decode_end(strm);
    return 0;
}

int check_zero(struct aec_stream *strm,
               uint8_t *ubuf,
               uint8_t *cbuf,
               uint8_t *obuf,
               size_t n)
{
    int bs, status;

    for (bs = 8; bs <= 64; bs *= 2) {
        memset(ubuf, 0x55, n);
        strm->bit_per_sample = 8;
        strm->block_size = bs;
        strm->rsi = n / bs;
        strm->flags = AEC_DATA_PREPROCESS;

        printf("Checking zero blocks with block size %i ... ", bs);

        status = encode_decode(strm, ubuf, cbuf, obuf, n);
        if (status)
            return status;

        if ((cbuf[0] & 0xf0) != 0) {
            printf("FAIL: Unexpected block created.\n");
            return 99;
        }
        printf ("OK\n");
    }
    return 0;
}

int main (void)
{
    int status;
    uint8_t *ubuf, *cbuf, *obuf;
    struct aec_stream strm;

    ubuf = (uint8_t *)malloc(BUF_SIZE);
    cbuf = (uint8_t *)malloc(BUF_SIZE);
    obuf = (uint8_t *)malloc(BUF_SIZE);

    if (!ubuf || !cbuf || !obuf) {
        printf("Not enough memory.\n");
        return 99;
    }

    status = check_zero(&strm, ubuf, cbuf, obuf, BUF_SIZE);
    if (status)
        return status;

    free(ubuf);
    free(cbuf);
    free(obuf);

    return 0;
}
