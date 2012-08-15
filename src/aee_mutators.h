#ifndef AEE_MUTATORS_H
#define AEE_MUTATORS_H

#include <inttypes.h>
#include "libae.h"

int64_t get_lsb_32(ae_streamp);
int64_t get_lsb_16(ae_streamp);
int64_t get_msb_32(ae_streamp);
int64_t get_msb_16(ae_streamp);
int64_t get_8(ae_streamp);

void get_block_msb_16_bs_8(ae_streamp);
void get_block_msb_16_bs_16(ae_streamp);
void get_block_8_bs_8(ae_streamp);
void get_block_8_bs_16(ae_streamp);

#endif
