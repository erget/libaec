/**
 * @file encode_accessors.c
 *
 * @author Mathis Rosenhauer, Deutsches Klimarechenzentrum
 * @author Moritz Hanke, Deutsches Klimarechenzentrum
 * @author Joerg Behrens, Deutsches Klimarechenzentrum
 * @author Luis Kornblueh, Max-Planck-Institut fuer Meteorologie
 *
 * @section LICENSE
 * Copyright 2012
 *
 * Mathis Rosenhauer,                 Luis Kornblueh
 * Moritz Hanke,
 * Joerg Behrens
 *
 * Deutsches Klimarechenzentrum GmbH  Max-Planck-Institut fuer Meteorologie
 * Bundesstr. 45a                     Bundesstr. 53
 * 20146 Hamburg                      20146 Hamburg
 * Germany                            Germany
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @section DESCRIPTION
 *
 * Read various data types from input stream
 *
 */

#include <config.h>

#if HAVE_STDINT_H
# include <stdint.h>
#endif

#include <string.h>
#include "libaec.h"
#include "encode.h"
#include "encode_accessors.h"

uint32_t get_8(struct aec_stream *strm)
{
    strm->avail_in--;
    strm->total_in++;
    return *strm->next_in++;
}

uint32_t get_lsb_24(struct aec_stream *strm)
{
    uint32_t data;

    data = ((uint32_t)strm->next_in[2] << 16)
        | ((uint32_t)strm->next_in[1] << 8)
        | (uint32_t)strm->next_in[0];

    strm->next_in += 3;
    strm->total_in += 3;
    strm->avail_in -= 3;
    return data;
}

uint32_t get_msb_24(struct aec_stream *strm)
{
    uint32_t data;

    data = ((uint32_t)strm->next_in[0] << 16)
        | ((uint32_t)strm->next_in[1] << 8)
        | (uint32_t)strm->next_in[2];

    strm->next_in += 3;
    strm->total_in += 3;
    strm->avail_in -= 3;
    return data;
}

#define GET_NATIVE_16(BO)                       \
uint32_t get_##BO##_16(struct aec_stream *strm) \
{                                               \
    uint32_t data;                              \
                                                \
    data = *(uint16_t *)strm->next_in;          \
    strm->next_in += 2;                         \
    strm->total_in += 2;                        \
    strm->avail_in -= 2;                        \
    return data;                                \
}

#define GET_NATIVE_32(BO)                       \
uint32_t get_##BO##_32(struct aec_stream *strm) \
{                                               \
    uint32_t data;                              \
                                                \
    data = *(uint32_t *)strm->next_in;          \
    strm->next_in += 4;                         \
    strm->total_in += 4;                        \
    strm->avail_in -= 4;                        \
    return data;                                \
}

#ifdef WORDS_BIGENDIAN
uint32_t get_lsb_16(struct aec_stream *strm)
{
    uint32_t data;

    data = ((uint32_t)strm->next_in[1] << 8)
        | (uint32_t)strm->next_in[0];

    strm->next_in += 2;
    strm->total_in += 2;
    strm->avail_in -= 2;
    return data;
}

uint32_t get_lsb_32(struct aec_stream *strm)
{
    uint32_t data;

    data = ((uint32_t)strm->next_in[3] << 24)
        | ((uint32_t)strm->next_in[2] << 16)
        | ((uint32_t)strm->next_in[1] << 8)
        | (uint32_t)strm->next_in[0];

    strm->next_in += 4;
    strm->total_in += 4;
    strm->avail_in -= 4;
    return data;
}

GET_NATIVE_16(msb);
GET_NATIVE_32(msb);

#else /* !WORDS_BIGENDIAN */
uint32_t get_msb_16(struct aec_stream *strm)
{
    uint32_t data;

    data = ((uint32_t)strm->next_in[0] << 8)
        | (uint32_t)strm->next_in[1];

    strm->next_in += 2;
    strm->total_in += 2;
    strm->avail_in -= 2;
    return data;
}

uint32_t get_msb_32(struct aec_stream *strm)
{
    uint32_t data;

    data = ((uint32_t)strm->next_in[0] << 24)
        | ((uint32_t)strm->next_in[1] << 16)
        | ((uint32_t)strm->next_in[2] << 8)
        | (uint32_t)strm->next_in[3];

    strm->next_in += 4;
    strm->total_in += 4;
    strm->avail_in -= 4;
    return data;
}

GET_NATIVE_16(lsb);
GET_NATIVE_32(lsb);

#endif /* !WORDS_BIGENDIAN */

void get_rsi_8(struct aec_stream *strm)
{
    uint32_t *out = strm->state->data_raw;
    unsigned const char *in = strm->next_in;
    int rsi = strm->rsi * strm->block_size;

    strm->next_in += rsi;
    strm->total_in += rsi;
    strm->avail_in -= rsi;

    while (rsi) {
        out[0] = (uint32_t)in[0];
        out[1] = (uint32_t)in[1];
        out[2] = (uint32_t)in[2];
        out[3] = (uint32_t)in[3];
        out[4] = (uint32_t)in[4];
        out[5] = (uint32_t)in[5];
        out[6] = (uint32_t)in[6];
        out[7] = (uint32_t)in[7];
        in += 8;
        out += 8;
        rsi -= 8;
    }
}

void get_rsi_lsb_24(struct aec_stream *strm)
{
    uint32_t *out = strm->state->data_raw;
    unsigned const char *in = strm->next_in;
    int rsi = strm->rsi * strm->block_size;

    strm->next_in += 3 * rsi;
    strm->total_in += 3 * rsi;
    strm->avail_in -= 3 * rsi;

    while (rsi) {
        out[0] = (uint32_t)in[0]
            | ((uint32_t)in[1] << 8)
            | ((uint32_t)in[2] << 16);
        out[1] = (uint32_t)in[3]
            | ((uint32_t)in[4] << 8)
            | ((uint32_t)in[5] << 16);
        out[2] = (uint32_t)in[6]
            | ((uint32_t)in[7] << 8)
            | ((uint32_t)in[8] << 16);
        out[3] = (uint32_t)in[9]
            | ((uint32_t)in[10] << 8)
            | ((uint32_t)in[11] << 16);
        out[4] = (uint32_t)in[12]
            | ((uint32_t)in[13] << 8)
            | ((uint32_t)in[14] << 16);
        out[5] = (uint32_t)in[15]
            | ((uint32_t)in[16] << 8)
            | ((uint32_t)in[17] << 16);
        out[6] = (uint32_t)in[18]
            | ((uint32_t)in[19] << 8)
            | ((uint32_t)in[20] << 16);
        out[7] = (uint32_t)in[21]
            | ((uint32_t)in[22] << 8)
            | ((uint32_t)in[23] << 16);
        in += 24;
        out += 8;
        rsi -= 8;
    }
}

void get_rsi_msb_24(struct aec_stream *strm)
{
    uint32_t *out = strm->state->data_raw;
    unsigned const char *in = strm->next_in;
    int rsi = strm->rsi * strm->block_size;

    strm->next_in += 3 * rsi;
    strm->total_in += 3 * rsi;
    strm->avail_in -= 3 * rsi;

    while (rsi) {
        out[0] = ((uint32_t)in[0] << 16)
            | ((uint32_t)in[1] << 8)
            | (uint32_t)in[2];
        out[1] = ((uint32_t)in[3] << 16)
            | ((uint32_t)in[4] << 8)
            | (uint32_t)in[5];
        out[2] = ((uint32_t)in[6] << 16)
            | ((uint32_t)in[7] << 8)
            | (uint32_t)in[8];
        out[3] = ((uint32_t)in[9] << 16)
            | ((uint32_t)in[10] << 8)
            | (uint32_t)in[11];
        out[4] = ((uint32_t)in[12] << 16)
            | ((uint32_t)in[13] << 8)
            | (uint32_t)in[14];
        out[5] = ((uint32_t)in[15] << 16)
            | ((uint32_t)in[16] << 8)
            | (uint32_t)in[17];
        out[6] = ((uint32_t)in[18] << 16)
            | ((uint32_t)in[19] << 8)
            | (uint32_t)in[20];
        out[7] = ((uint32_t)in[21] << 16)
            | ((uint32_t)in[22] << 8)
            | (uint32_t)in[23];
        in += 24;
        out += 8;
        rsi -= 8;
    }
}

#define GET_RSI_NATIVE_16(BO)                       \
    void get_rsi_##BO##_16(struct aec_stream *strm) \
    {                                               \
        uint32_t *out = strm->state->data_raw;      \
        uint16_t *in = (uint16_t *)strm->next_in;   \
        int rsi = strm->rsi * strm->block_size;     \
                                                    \
        strm->next_in += 2 * rsi;                   \
        strm->total_in += 2 * rsi;                  \
        strm->avail_in -= 2 * rsi;                  \
                                                    \
        while (rsi) {                               \
            out[0] = (uint32_t)in[0];               \
            out[1] = (uint32_t)in[1];               \
            out[2] = (uint32_t)in[2];               \
            out[3] = (uint32_t)in[3];               \
            out[4] = (uint32_t)in[4];               \
            out[5] = (uint32_t)in[5];               \
            out[6] = (uint32_t)in[6];               \
            out[7] = (uint32_t)in[7];               \
            in += 8;                                \
            out += 8;                               \
            rsi -= 8;                               \
        }                                           \
    }

#define GET_RSI_NATIVE_32(BO)                       \
    void get_rsi_##BO##_32(struct aec_stream *strm) \
    {                                               \
        int rsi = strm->rsi * strm->block_size;     \
        memcpy(strm->state->data_raw,               \
               strm->next_in, 4 * rsi);             \
        strm->next_in += 4 * rsi;                   \
        strm->total_in += 4 * rsi;                  \
        strm->avail_in -= 4 * rsi;                  \
    }

#ifdef WORDS_BIGENDIAN
void get_rsi_lsb_16(struct aec_stream *strm)
{
    uint32_t *out = strm->state->data_raw;
    unsigned const char *in = strm->next_in;
    int rsi = strm->rsi * strm->block_size;

    strm->next_in += 2 * rsi;
    strm->total_in += 2 * rsi;
    strm->avail_in -= 2 * rsi;

    while (rsi) {
        out[0] = (uint32_t)in[0]
            | ((uint32_t)in[1] << 8);
        out[1] = (uint32_t)in[2]
            | ((uint32_t)in[3] << 8);
        out[2] = (uint32_t)in[4]
            | ((uint32_t)in[5] << 8);
        out[3] = (uint32_t)in[6]
            | ((uint32_t)in[7] << 8);
        out[4] = (uint32_t)in[8]
            | ((uint32_t)in[9] << 8);
        out[5] = (uint32_t)in[10]
            | ((uint32_t)in[11] << 8);
        out[6] = (uint32_t)in[12]
            | ((uint32_t)in[13] << 8);
        out[7] = (uint32_t)in[14]
            | ((uint32_t)in[15] << 8);
        in += 16;
        out += 8;
        rsi -= 8;
    }
}

void get_rsi_lsb_32(struct aec_stream *strm)
{
    uint32_t *out = strm->state->data_raw;
    unsigned const char *in = strm->next_in;
    int rsi = strm->rsi * strm->block_size;

    strm->next_in += 4 * rsi;
    strm->total_in += 4 * rsi;
    strm->avail_in -= 4 * rsi;

    while (rsi) {
        out[0] = (uint32_t)in[0]
            | ((uint32_t)in[1] << 8)
            | ((uint32_t)in[2] << 16)
            | ((uint32_t)in[3] << 24);
        out[1] = (uint32_t)in[4]
            | ((uint32_t)in[5] << 8)
            | ((uint32_t)in[6] << 16)
            | ((uint32_t)in[7] << 24);
        out[2] = (uint32_t)in[8]
            | ((uint32_t)in[9] << 8)
            | ((uint32_t)in[10] << 16)
            | ((uint32_t)in[11] << 24);
        out[3] = (uint32_t)in[12]
            | ((uint32_t)in[13] << 8)
            | ((uint32_t)in[14] << 16)
            | ((uint32_t)in[15] << 24);
        out[4] = (uint32_t)in[16]
            | ((uint32_t)in[17] << 8)
            | ((uint32_t)in[18] << 16)
            | ((uint32_t)in[19] << 24);
        out[5] = (uint32_t)in[20]
            | ((uint32_t)in[21] << 8)
            | ((uint32_t)in[22] << 16)
            | ((uint32_t)in[23] << 24);
        out[6] = (uint32_t)in[24]
            | ((uint32_t)in[25] << 8)
            | ((uint32_t)in[26] << 16)
            | ((uint32_t)in[27] << 24);
        out[7] = (uint32_t)in[28]
            | ((uint32_t)in[29] << 8)
            | ((uint32_t)in[30] << 16)
            | ((uint32_t)in[31] << 24);
        in += 32;
        out += 8;
        rsi -= 8;
    }
}

GET_RSI_NATIVE_16(msb);
GET_RSI_NATIVE_32(msb);

#else /* !WORDS_BIGENDIAN */
void get_rsi_msb_16(struct aec_stream *strm)
{
    uint32_t *out = strm->state->data_raw;
    unsigned const char *in = strm->next_in;
    int rsi = strm->rsi * strm->block_size;

    strm->next_in += 2 * rsi;
    strm->total_in += 2 * rsi;
    strm->avail_in -= 2 * rsi;

    while (rsi) {
        out[0] = ((uint32_t)in[0] << 8)
            | (uint32_t)in[1];
        out[1] = ((uint32_t)in[2] << 8)
            | (uint32_t)in[3];
        out[2] = ((uint32_t)in[4] << 8)
            | (uint32_t)in[5];
        out[3] = ((uint32_t)in[6] << 8)
            | (uint32_t)in[7];
        out[4] = ((uint32_t)in[8] << 8)
            | (uint32_t)in[9];
        out[5] = ((uint32_t)in[10] << 8)
            | (uint32_t)in[11];
        out[6] = ((uint32_t)in[12] << 8)
            | (uint32_t)in[13];
        out[7] = ((uint32_t)in[14] << 8)
            | (uint32_t)in[15];
        in += 16;
        out += 8;
        rsi -= 8;
    }
}

void get_rsi_msb_32(struct aec_stream *strm)
{
    uint32_t *out = strm->state->data_raw;
    unsigned const char *in = strm->next_in;
    int rsi = strm->rsi * strm->block_size;

    strm->next_in += 4 * rsi;
    strm->total_in += 4 * rsi;
    strm->avail_in -= 4 * rsi;

    while (rsi) {
        out[0] = ((uint32_t)in[0] << 24)
            | ((uint32_t)in[1] << 16)
            | ((uint32_t)in[2] << 8)
            | (uint32_t)in[3];
        out[1] = ((uint32_t)in[4] << 24)
            | ((uint32_t)in[5] << 16)
            | ((uint32_t)in[6] << 8)
            | (uint32_t)in[7];
        out[2] = ((uint32_t)in[8] << 24)
            | ((uint32_t)in[9] << 16)
            | ((uint32_t)in[10] << 8)
            | (uint32_t)in[11];
        out[3] = ((uint32_t)in[12] << 24)
            | ((uint32_t)in[13] << 16)
            | ((uint32_t)in[14] << 8)
            | (uint32_t)in[15];
        out[4] = ((uint32_t)in[16] << 24)
            | ((uint32_t)in[17] << 16)
            | ((uint32_t)in[18] << 8)
            | (uint32_t)in[19];
        out[5] = ((uint32_t)in[20] << 24)
            | ((uint32_t)in[21] << 16)
            | ((uint32_t)in[22] << 8)
            | (uint32_t)in[23];
        out[6] = ((uint32_t)in[24] << 24)
            | ((uint32_t)in[25] << 16)
            | ((uint32_t)in[26] << 8)
            | (uint32_t)in[27];
        out[7] = ((uint32_t)in[28] << 24)
            | ((uint32_t)in[29] << 16)
            | ((uint32_t)in[30] << 8)
            | (uint32_t)in[31];
        in += 32;
        out += 8;
        rsi -= 8;
    }
}

GET_RSI_NATIVE_16(lsb);
GET_RSI_NATIVE_32(lsb);

#endif /* !WORDS_BIGENDIAN */
