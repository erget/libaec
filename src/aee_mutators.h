#ifndef AEE_MUTATORS_H
#define AEE_MUTATORS_H

#include <inttypes.h>
#include "libae.h"

uint32_t get_lsb_32(ae_streamp strm);
uint32_t get_lsb_16(ae_streamp strm);
uint32_t get_msb_32(ae_streamp strm);
uint32_t get_msb_16(ae_streamp strm);
uint32_t get_8(ae_streamp strm);

void get_block_msb_32(ae_streamp strm);
void get_block_msb_16_bs_8(ae_streamp strm);
void get_block_msb_16(ae_streamp strm);
void get_block_8_bs_8(ae_streamp strm);
void get_block_8(ae_streamp strm);

#endif
