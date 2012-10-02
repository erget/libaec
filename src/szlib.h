#ifndef SZLIB_H
#define SZLIB_H

#include "libaec.h"

#define SZ_OK AEC_OK
#define SZ_NO_ENCODER_ERROR -1
#define SZ_PARAM_ERROR AEC_CONF_ERROR
#define SZ_MEM_ERROR AEC_MEM_ERROR
#define SZ_OUTBUFF_FULL -2

#define SZ_RAW_OPTION_MASK 128
#define SZ_NN_OPTION_MASK AEC_DATA_PREPROCESS
#define SZ_LSB_OPTION_MASK AEC_DATA_LSB
#define SZ_MSB_OPTION_MASK AEC_DATA_MSB

typedef struct SZ_com_t_s
{
    int options_mask;
    int bits_per_pixel;
    int pixels_per_block;
    int pixels_per_scanline;
} SZ_com_t;

int SZ_BufftoBuffCompress(void *dest, size_t *destLen,
                          const void *source, size_t sourceLen,
                          SZ_com_t *param);
int SZ_BufftoBuffDecompress(void *dest, size_t *destLen,
                            const void *source, size_t sourceLen,
                            SZ_com_t *param);

#endif /* SZLIB_H */
