#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2022, Intel Corporation. All rights reserved.

#pylint:disable=mixed-indentation

# Tool to stream data from Linux SOF driver "mtrace" debugfs
# interface to standard output. Plain "cat" is not sufficient
# as each read() syscall returns log data with a 32bit binary
# header, containing the payload length.

import struct
import os
import sys
import argparse

READ_BUFFER = 16384
MTRACE_FILE = "/sys/kernel/debug/sof/mtrace/core0"

parser = argparse.ArgumentParser()
parser.add_argument('-m', '--mark-chunks',
                    action='store_true')

args = parser.parse_args()

chunk_idx = 0

fd = os.open(MTRACE_FILE, os.O_RDONLY)
while fd >= 0:
    # direct unbuffered os.read() must be used to comply with
    # debugfs protocol used. each non-zero read will return
    # a buffer containing a 32bit header and a payload
    read_bytes = os.read(fd, READ_BUFFER)

    # handle end-of-file
    if len(read_bytes) == 0:
        continue

    if len(read_bytes) <= 4:
        continue

    header = struct.unpack('I', read_bytes[0:4])
    data_len = header[0]
    data = read_bytes[4:4+data_len]

    if (args.mark_chunks):
        chunk_msg = "\n--- Chunk #{} start (size: {}) ---\n" .format(chunk_idx, data_len)
        sys.stdout.write(chunk_msg)

    sys.stdout.buffer.write(data)
    sys.stdout.flush()
    chunk_idx += 1
