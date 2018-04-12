/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sof/module.h>
#include <arch/reloc.h>

int main(int argc, char *argv[])
{
	struct reloc reloc;
	size_t count;
	int ret = 0;
	long file_size;

	bzero(&reloc, sizeof(reloc));

	/* open the elf input file */
	reloc.fd = fopen(argv[1], "r");
	if (reloc.fd == NULL) {
		fprintf(stderr, "error: unable to open %s for reading %d\n",
				argv[1], errno);
		return -EINVAL;
	}

	fseek(reloc.fd, 0, SEEK_END);
	file_size = ftell(reloc.fd);

	reloc.elf = calloc(file_size, 1);
	if (reloc.elf == NULL)
		return -errno;

	/* read in elf header */
	fseek(reloc.fd, 0, SEEK_SET);
	count = fread(reloc.elf, file_size, 1, reloc.fd);
	if (count != 1) {
		fprintf(stderr, "error: failed to read %s elf file %d\n",
			argv[1], -errno);
		return -errno;
	}

	/* read sections */
	ret = reloc_mod(&reloc);
	if (ret < 0) {
		fprintf(stderr, "error: failed to read base sections %d\n",
			ret);
	}

	return 0;

hdr_err:
	fclose(reloc.fd);

	return ret;
}
