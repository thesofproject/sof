// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.
//
// Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>
//
// Compile instructions:
//   gcc -o mtrace-reader mtrace-reader.c -Wall -O2
//

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>

#define READ_BUFFER 16384
#define MTRACE_FILE "/sys/kernel/debug/sof/mtrace/core0"

uint8_t buffer[READ_BUFFER];

int main(void)
{
	ssize_t read_bytes;
	uint32_t data_len;
	uint32_t header;
	int fd;

	fd = open(MTRACE_FILE, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	while (1) {
		read_bytes = read(fd, buffer, READ_BUFFER);

		/* handle end-of-file */
		if (read_bytes == 0)
			continue;

		if (read_bytes <= 4)
			continue;

		header = *(uint32_t *)buffer;
		data_len = header;
		if (data_len > read_bytes - 4)
			continue;

		if (write(STDOUT_FILENO, buffer + 4, data_len) < 0) {
			perror("write");
			return -1;
		}
	}

	close(fd);

	return 0;
}
