#ifndef ENCODE_ACCESSORS_H
#define ENCODE_ACCESSORS_H

#include <config.h>

#if HAVE_STDINT_H
# include <stdint.h>
#endif

#include "libaec.h"

uint32_t get_8(struct aec_stream *strm);
uint32_t get_lsb_16(struct aec_stream *strm);
uint32_t get_msb_16(struct aec_stream *strm);
uint32_t get_lsb_32(struct aec_stream *strm);
uint32_t get_msb_24(struct aec_stream *strm);
uint32_t get_lsb_24(struct aec_stream *strm);
uint32_t get_msb_32(struct aec_stream *strm);

void get_rsi_8(struct aec_stream *strm);
void get_rsi_lsb_16(struct aec_stream *strm);
void get_rsi_msb_16(struct aec_stream *strm);
void get_rsi_lsb_24(struct aec_stream *strm);
void get_rsi_msb_24(struct aec_stream *strm);
void get_rsi_lsb_32(struct aec_stream *strm);
void get_rsi_msb_32(struct aec_stream *strm);

#endif /* ENCODE_ACCESSORS_H */
