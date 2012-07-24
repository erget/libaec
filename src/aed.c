/* Adaptive Entropy Decoder            */
/* CCSDS 121.0-B-1 and CCSDS 120.0-G-2 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include "aecd.h"

#define ASK(n)											\
    do {												\
		while (state->bitp < (unsigned)(n))				\
		{												\
			if (strm->avail_in == 0) goto req_buffer;	\
			strm->avail_in--;							\
			strm->total_in++;							\
			state->acc <<= 8;							\
			state->acc |= (uint64_t)(*strm->next_in++);	\
			state->bitp += 8;							\
		}												\
	} while (0)
	  
#define GET(n)													\
    ((state->acc >> (state->bitp - (n))) & ((1ULL << (n)) - 1))

#define DROP(n)						  \
    do {							  \
        state->bitp -= (unsigned)(n); \
    } while (0)

#define ASKFS()							 \
	do {								 \
		ASK(1);							 \
		state->fs = 0;					 \
		while (GET(state->fs + 1) == 0)	 \
		{								 \
			state->fs++;				 \
			ASK(state->fs + 1);			 \
		}								 \
	} while(0)

#define GETFS()	\
	state->fs

#define DROPFS()			 \
	do {					 \
		DROP(state->fs + 1); \
	} while(0)

#define REFBLOCK(strm) (strm->pp && (strm->total_out / strm->block_size) \
						% strm->segment_size == 0)
#define ROS 5

typedef struct internal_state {
	uint32_t id_len;   /* bit length of code option identification key */
	int *id_table;     /* table maps IDs to states */
	size_t ref_int;    /* reference sample is every ref_int samples */ 
	uint32_t last_out; /* previous output for post-processing */
	int64_t xmin;      /* minimum integer for post-processing */
	int64_t xmax;      /* maximum integer for post-processing */
	int mode;          /* current mode of FSM */
	int pushed_mode;   /* originating mode for generic modes */
	size_t count, i;   /* total number of samples in block and current sample */
	int k;             /* k for split-sample options */
	uint32_t *block;   /* block buffer for split-sample options */
	uint64_t acc;      /* accumulator for currently used bit sequence */
	uint8_t bitp;      /* bit pointer to the next unused bit in accumulator */
	uint32_t fs;       /* last fundamental sequence in accumulator */
	uint32_t delta1;   /* interim result we need to keep for SE option */
} decode_state;

/* decoding table for the second-extension option */
static const uint32_t second_extension[36][2] = {
	{0, 0},
	{1, 1}, {1, 1},
	{2, 3}, {2, 3}, {2, 3},
	{3, 6}, {3, 6}, {3, 6}, {3, 6},
	{4, 10}, {4, 10}, {4, 10}, {4, 10}, {4, 10},
	{5, 15}, {5, 15}, {5, 15}, {5, 15}, {5, 15}, {5, 15},
	{6, 21}, {6, 21}, {6, 21}, {6, 21}, {6, 21}, {6, 21}, {6, 21},
	{7, 28}, {7, 28}, {7, 28}, {7, 28}, {7, 28}, {7, 28}, {7, 28}, {7, 28}
};

enum
{
	M_ID = 0,
	M_SPLIT,
	M_SPLIT_FS,
	M_SPLIT_BITS,
	M_SPLIT_OUTPUT,
	M_LOW_ENTROPY,
	M_ZERO_BLOCK,
	M_SE,
	M_SE_DECODE,
	M_GAMMA_GET,
	M_GAMMA_OUTPUT_0,
	M_GAMMA_OUTPUT_1,
	M_ZERO_OUTPUT,
	M_UNCOMP,
	M_UNCOMP_COPY,
	M_SAMPLE_GET,
	M_SAMPLE_OUTPUT
};

static inline int output(ae_streamp strm, uint32_t out)
{
	/**
	   Outputs a post-processed sample.

	   If no post-processor is present then output unaltered.
	 */

	int64_t x, d, th, D;
	decode_state *state;

	if (strm->avail_out == 0)
	{
		return AE_STREAM_END;
	}

	state = strm->state;
	if (strm->pp && (strm->total_out % state->ref_int != 0))
	{
		d = out;
		x = state->last_out;

		if ((x - state->xmin) < (state->xmax - x))
		{
			th = x - state->xmin;
		}
		else
		{
			th = state->xmax - x;
		}

		if (d <= 2*th)
		{
			if (d & 1)
				D = - (d + 1) / 2;
			else
				D = d / 2;
		} else {
			if (th == x)
				D = d - th;
			else
				D = th - d;
		}
		out = x + D;
	}
	*strm->next_out++ = state->last_out = out;
	strm->avail_out--;
	strm->total_out++;
	return AE_OK;
}

int ae_decode_init(ae_streamp strm)
{
	int i, modi;
	decode_state *state;

	/* Some sanity checks */
	if (strm->bit_per_sample > 32 || strm->bit_per_sample == 0)
	{
		return AE_ERRNO;
	}

	/* Internal state for decoder */
	state = (decode_state *) malloc(sizeof(decode_state));
	if (state == NULL)
	{
		return AE_MEM_ERROR;
	}
	strm->state = state;

	if (16 < strm->bit_per_sample)
		state->id_len = 5;
	else if (8 < strm->bit_per_sample)
		state->id_len = 4;
	else
		state->id_len = 3;

	state->ref_int = strm->block_size * strm->segment_size;

	modi = 1UL << state->id_len;
	state->id_table = (int *)malloc(modi * sizeof(int));
	if (state->id_table == NULL)
	{
		return AE_MEM_ERROR;
	}
	state->id_table[0] = M_LOW_ENTROPY;
	for (i = 1; i < modi - 1; i++)
	{
		state->id_table[i] = M_SPLIT;
	}
	state->id_table[modi - 1] = M_UNCOMP;

	state->block = (uint32_t *)malloc(strm->block_size * sizeof(uint32_t));
	if (state->block == NULL)
	{
		return AE_MEM_ERROR;
	}
	strm->total_in = 0;
	strm->total_out = 0;
	state->xmin = 0;
	state->xmax = (1ULL << strm->bit_per_sample) - 1;

	state->bitp = 0;
	state->mode = M_ID;
	return AE_OK;
}

int ae_decode(ae_streamp strm, int flush)
{
	int id;
	size_t zero_blocks;
	uint32_t gamma, beta, ms;
	decode_state *state;

	state = strm->state;

	for (;;)
	{
		/* Slow but restartable finite-state machine implementation
		   of the adaptive entropy decoder. Can work with one byte
		   input und one sample output buffers. Inspired by zlib.

		   TODO: Fast version with prior buffer size checking.
                 Flush modes like in zlib
		 */
		switch(state->mode)
		{
		case M_ID:
			ASK(3);
			id = GET(3);
			DROP(3);
			state->mode = state->id_table[id];
			state->k = id - 1; 
			break;

		case M_SPLIT:
			state->count = strm->block_size;
			state->i = 0;
			state->mode = M_SPLIT_FS;
			if (REFBLOCK(strm))
			{
				state->pushed_mode = M_SPLIT_FS;
				state->mode = M_SAMPLE_GET;
				state->count--;
				break;
			}

		case M_SPLIT_FS:
			while(state->i < state->count)
			{
				ASKFS();
				state->block[state->i] = GETFS() << state->k;
				DROPFS();
				state->i++;
			}
			state->i = 0;
			state->mode = M_SPLIT_BITS;

		case M_SPLIT_BITS:
			while(state->i < state->count)
			{
				ASK(state->k);
				state->block[state->i] |= GET(state->k);
				DROP(state->k);
				state->i++;
			}
			state->i = 0;
			state->mode = M_SPLIT_OUTPUT;

		case M_SPLIT_OUTPUT:
			while(state->i < state->count)
			{
				if (output(strm, state->block[state->i]) == AE_OK)
				{
					state->i++;
				}
				else
				{
					goto req_buffer;
				}
			}
			state->mode = M_ID;
			break;

		case M_LOW_ENTROPY:
			ASK(1);
			if(GET(1))
			{
				state->mode = M_SE;
			}
			else
			{
				state->mode = M_ZERO_BLOCK;
			}
			DROP(1);
			if (REFBLOCK(strm))
			{
				state->pushed_mode = state->mode;
				state->mode = M_SAMPLE_GET;
			}
			break;

		case M_ZERO_BLOCK:
			ASKFS();
			zero_blocks = GETFS() + 1;
			DROPFS();
			if (zero_blocks == ROS)
			{
				zero_blocks =  strm->segment_size - (
					(strm->total_out / strm->block_size) 
					% strm->segment_size);
			}
			state->count = zero_blocks * strm->block_size;
			if (REFBLOCK(strm))
			{
				state->count--;
			}
			state->mode = M_ZERO_OUTPUT;

		case M_ZERO_OUTPUT:
			while(state->count > 0 && output(strm, 0) == AE_OK)
			{
				state->count--;
			}
			if (state->count == 0)
			{
				state->mode = M_ID;
			}
			else
			{
				goto req_buffer;
			}
			break;

		case M_SE:
			state->count = strm->bit_per_sample / 2;
			state->mode = M_SE_DECODE;

		case M_SE_DECODE:
			if(state->count > 0)
			{
				state->count--;
				state->mode = M_GAMMA_GET;
			}
			else
			{
				state->mode = M_ID;
				break;
			}
		case M_GAMMA_GET:
			ASKFS();
			state->mode = M_GAMMA_OUTPUT_0;

		case M_GAMMA_OUTPUT_0:
			gamma = GETFS();
			beta = second_extension[gamma][0];
			ms = second_extension[gamma][1];
			state->delta1 = gamma - ms;
			if (!(REFBLOCK(strm) && state->count == strm->bit_per_sample / 2 - 1))
			{
				if (output(strm, beta - state->delta1) != AE_OK)
					goto req_buffer;
			}
			DROPFS();
			state->mode = M_GAMMA_OUTPUT_1;

		case M_GAMMA_OUTPUT_1:
			if (output(strm, state->delta1) != AE_OK)
				goto req_buffer;

			state->mode = M_SE_DECODE;
			break;
			
		case M_UNCOMP:
			state->count = strm->block_size;
			state->mode = M_UNCOMP_COPY;

		case M_UNCOMP_COPY:
			if(state->count > 0)
			{
				state->count--;
				state->pushed_mode = M_UNCOMP_COPY;
				state->mode = M_SAMPLE_GET;
			}
			else
			{
				state->mode = M_ID;
			}
			break;
			
		case M_SAMPLE_GET:
			ASK(strm->bit_per_sample);
			state->mode = M_SAMPLE_OUTPUT;

		case M_SAMPLE_OUTPUT:
			if (output(strm, GET(strm->bit_per_sample)) == AE_OK)
			{
				DROP(strm->bit_per_sample);
				state->mode = state->pushed_mode;
			}
			else
			{
				goto req_buffer;
			}
			break;

		default:
			return AE_STREAM_ERROR;
		}
	}

req_buffer:
	return AE_OK;
}
