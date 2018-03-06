/*
	LZ4 - Fast LZ compression algorithm
	Copyright (C) 2011, Yann Collet.
	BSD License
	Matteo Croce <mcroce@redhat.com> - Added standalone support, ported to liblz4

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are
	met:

	* Redistributions of source code must retain the above copyright
	notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above
	copyright notice, this list of conditions and the following disclaimer
	in the documentation and/or other materials provided with the
	distribution.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
	A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
	OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
	SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
	LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
//**************************************
// Includes
//**************************************
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <lz4.h>

#define LZ4P_MAGIC	"LZ4P"

#define LZ4_E_INPUT_OVERRUN	    (-4)

#define unlz4_check_input_size(size)		\
	while (in_len < (size))			\
	{					\
		ret = fill(fill_data);		\
		if (ret < 0)			\
		return LZ4_E_INPUT_OVERRUN;	\
		in_len += ret;			\
	}

typedef struct
{
	uint32_t	magic;		// 'L','Z','4','P'
	uint32_t	osize;		// original size
	uint32_t	csize;		// compressed size
	uint32_t	bsize;		// block size
	uint32_t	nblock;		// block count
	uint32_t	reserved[3];
} lz4p_header_t;

static unsigned int unlz4_get_decompsize(unsigned char * buf)
{
	lz4p_header_t * lz4p_header = (lz4p_header_t * )buf ;
	return (unsigned int)(lz4p_header->osize);
}

static int unlz4_read(uint8_t *input, int in_len,
				int (*fill) (void *fill_data),
				void *fill_data,
				uint8_t *output, int *posp)
{
	uint8_t *in_buf, *out_buf;
	int sink_int, next_size;
	int ret = -1;

	lz4p_header_t *header;
	unsigned int block_size;

	uint32_t	n, block_count, blocklist_size;
	uint32_t*	list_block_size;

	out_buf = output;
	in_buf  = input;

	if (*posp)
		*posp = 0;

	// check header...
	unlz4_check_input_size(sizeof(lz4p_header_t));

	header = (lz4p_header_t*)in_buf;
	if(memcmp(&header->magic, LZ4P_MAGIC, 4)) {
		puts("Unrecognized header : file cannot be decoded");
		return 6;
	}
	in_buf += sizeof(lz4p_header_t);
	in_len -= sizeof(lz4p_header_t);


	block_size = header->bsize;
	block_count = header->nblock;

	blocklist_size = sizeof(uint32_t) * block_count;
	list_block_size = (uint32_t*)malloc(blocklist_size);

	unlz4_check_input_size(blocklist_size);
	memcpy(list_block_size, in_buf, blocklist_size);
	in_buf += blocklist_size;
	in_len -= blocklist_size;

	for(n=0; n<block_count; n++)
	{
		next_size = list_block_size[n];
		unlz4_check_input_size(next_size);

		if(n < (block_count-1))
		{
			// Decode Block
			sink_int = LZ4_decompress_fast(in_buf, out_buf, block_size);
			if(sink_int < 0 || sink_int != next_size)
			{
				printf("Uncompress error. n:%d, res:%d, nextSize:%d\n",
						n, sink_int, next_size);
				return 8;
			}
			*posp += block_size;
		}
		else
		{
			// Last Block
			sink_int = LZ4_decompress_safe(in_buf, out_buf, next_size, block_size);
			if(sink_int < 0) {
				printf("Uncompress error : res=%d\n", sink_int);
				return 9;
			}
			*posp += sink_int;
			break;
		}

		in_buf += next_size;
		out_buf += block_size;
		in_len -= next_size;

	}

	free(list_block_size);

	return 0;
}

int main(int argc, char *argv[])
{
	char *bufferin, *bufferout;
	struct stat statbuf;
	int readed;
	FILE *in, *out;

	if (argc != 3) {
		fprintf(stderr, "usage: $0 <infile.lz4p> <outfile.dec>\n");
		return 1;
	}

	in = fopen(argv[1], "r");
	if (!in) {
		perror("fopen");
		return 2;
	}

	out = fopen(argv[2], "w+");
	if (!out) {
		perror("fopen");
		return 3;
	}

	if (fstat(fileno(in), &statbuf)) {
		perror("stat");
		return 4;
	}

	bufferin = malloc(statbuf.st_size);
	if (!bufferin) {
		perror("malloc");
		return 5;
	}

	readed = fread(bufferin, 1, statbuf.st_size, in);
	if (readed < statbuf.st_size) {
		perror("fread");
		return 6;
	}

	bufferout = malloc(unlz4_get_decompsize(bufferin));
	if (!bufferout) {
		perror("malloc");
		return 7;
	}

	if (unlz4_read(bufferin, readed, NULL, NULL, bufferout, &readed)) {
		fputs("unlz4_read: error", stderr);
		return 8;
	}

	fwrite(bufferout, 1, readed, out);

	return 0;
}
