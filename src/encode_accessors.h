#ifndef ENCODE_ACCESSORS_H
#define ENCODE_ACCESSORS_H

#include <inttypes.h>
#include "libaec.h"

uint32_t get_8(aec_streamp strm);
uint32_t get_lsb_16(aec_streamp strm);
uint32_t get_msb_16(aec_streamp strm);
uint32_t get_lsb_32(aec_streamp strm);
uint32_t get_msb_24(aec_streamp strm);
uint32_t get_lsb_24(aec_streamp strm);
uint32_t get_msb_32(aec_streamp strm);

extern void (*get_block_funcs_8[4])(aec_streamp);
extern void (*get_block_funcs_lsb_16[4])(aec_streamp);
extern void (*get_block_funcs_msb_16[4])(aec_streamp);
extern void (*get_block_funcs_lsb_24[4])(aec_streamp);
extern void (*get_block_funcs_msb_24[4])(aec_streamp);
extern void (*get_block_funcs_lsb_32[4])(aec_streamp);
extern void (*get_block_funcs_msb_32[4])(aec_streamp);

#endif /* ENCODE_ACCESSORS_H */
