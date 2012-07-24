#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include "aecd.h"

#define CHUNK_OUT 1
#define CHUNK_IN 1
#define ALL_IN 9478

int main(int argc, char *argv[])
{
	ae_stream strm;
	int c, i, n, status, todo;
	uint8_t *in;
	uint32_t *out;
	size_t total_out;

	in = (uint8_t *)malloc(ALL_IN);
	out = (uint32_t *)malloc(CHUNK_OUT * sizeof(uint32_t));
	if (in == NULL || out == NULL)
		return 1;

	n = 0;
	while ((c = getc(stdin)) != EOF)
	{
		*in++ = c;
		n++;
	}
	in -= n;

	strm.bit_per_sample = 8;
	strm.block_size = 8;
	strm.segment_size = 2;
	strm.pp = 1;

	if (ae_decode_init(&strm) != AE_OK)
		return 1;
	
	strm.next_in = in;
	strm.avail_in = CHUNK_IN;
	strm.next_out = out;
	strm.avail_out = CHUNK_OUT;
	todo = 1;
	total_out = 0;
	
	while(todo)
	{
		todo = 0;
		if ((status = ae_decode(&strm, 0)) != AE_OK)
		{
			fprintf(stderr, "error is %i\n", status);
			return 1;
		}
		fprintf(stderr, "avail in %li total in %li avail out %li total out %lx\n", strm.avail_in, strm.total_in, strm.avail_out, strm.total_out);

		if (strm.avail_in == 0 && strm.total_in < ALL_IN)
		{
			in += CHUNK_IN;

			strm.next_in = in;
			if (ALL_IN - strm.total_in < CHUNK_IN)
				strm.avail_in = ALL_IN - strm.total_in;
			else
				strm.avail_in = CHUNK_IN;
			todo = 1;
		}

		if (strm.total_out - total_out > 0)
		{
			for (i=0; i < strm.total_out - total_out; i++)
			{
				putc(out[i], stdout);
			}
			total_out = strm.total_out;
			strm.next_out = out;
			strm.avail_out = CHUNK_OUT;
			todo = 1;
		}
	}

	return 0;
}
