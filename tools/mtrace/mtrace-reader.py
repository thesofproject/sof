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

READ_BUFFER = 16384
MTRACE_FILE = "/sys/kernel/debug/sof/mtrace/core0"

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

    os.write(sys.stdout.fileno(), data)
