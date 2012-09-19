#ifndef ENCODE_ACCESSORS_H
#define ENCODE_ACCESSORS_H

#include <inttypes.h>
#include "libaec.h"

uint32_t get_8(struct aec_stream *strm);
uint32_t get_lsb_16(struct aec_stream *strm);
uint32_t get_msb_16(struct aec_stream *strm);
uint32_t get_lsb_32(struct aec_stream *strm);
uint32_t get_msb_24(struct aec_stream *strm);
uint32_t get_lsb_24(struct aec_stream *strm);
uint32_t get_msb_32(struct aec_stream *strm);

extern void (*get_block_funcs_8[4])(struct aec_stream *);
extern void (*get_block_funcs_lsb_16[4])(struct aec_stream *);
extern void (*get_block_funcs_msb_16[4])(struct aec_stream *);
extern void (*get_block_funcs_lsb_24[4])(struct aec_stream *);
extern void (*get_block_funcs_msb_24[4])(struct aec_stream *);
extern void (*get_block_funcs_lsb_32[4])(struct aec_stream *);
extern void (*get_block_funcs_msb_32[4])(struct aec_stream *);

#endif /* ENCODE_ACCESSORS_H */
