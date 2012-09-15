#ifndef AEE_ACCESSORS_H
#define AEE_ACCESSORS_H

#include <inttypes.h>
#include "libae.h"

uint32_t get_8(ae_streamp strm);
uint32_t get_lsb_16(ae_streamp strm);
uint32_t get_msb_16(ae_streamp strm);
uint32_t get_lsb_32(ae_streamp strm);
uint32_t get_msb_24(ae_streamp strm);
uint32_t get_lsb_24(ae_streamp strm);
uint32_t get_msb_32(ae_streamp strm);

extern void (*get_block_funcs_8[4])(ae_streamp);
extern void (*get_block_funcs_lsb_16[4])(ae_streamp);
extern void (*get_block_funcs_msb_16[4])(ae_streamp);
extern void (*get_block_funcs_lsb_24[4])(ae_streamp);
extern void (*get_block_funcs_msb_24[4])(ae_streamp);
extern void (*get_block_funcs_lsb_32[4])(ae_streamp);
extern void (*get_block_funcs_msb_32[4])(ae_streamp);

#endif
