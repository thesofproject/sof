#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2020, Intel Corporation. All rights reserved.
#
# Author: Jaska Uimonen <jaska.uimonen@linux.intel.com>

"""Examples to generate a blob for

mux/demux:
sof_gen_blob.py -a 3 14 0 -m 3H I 1B 8B 3B I 1B 8B 3B -v "2 2 2" "1" "2" "1 2 4 8 16 32 64 128" "0 0 0" "5" "1" "1 1 4 8 16 32 64 128" "0 0 0"

dc_blocker:
sof_gen_blob.py -a 3 14 0 -m 8I -v "1 2 3 4 5 6 7 8"

different types for fun and profit:
sof_gen_blob.py -a 3 14 0 -m f I 4s h -v "0.2" "1" "fish" "-2"
"""

import argparse
import struct
import sys

SOF_ABI_MAJOR_SHIFT = 24
SOF_ABI_MINOR_SHIFT = 12
SOF_ABI_PATCH_SHIFT = 0

items_per_line = 8

def conv_and_get_type(val):
    for count, c in enumerate([int, float, str], start=0):
        try:
            return c(val), count
        except ValueError:
            pass

parser = argparse.ArgumentParser(description='Print C struct as hex blob for alsa conf.', epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--abiver", "-a", type=int, nargs=3, help='sof abi version major minor patch')
parser.add_argument('--controlid', "-c", type=int, nargs=1, help='define controlid and wrap the blob around snd_ctl_tlv header')
parser.add_argument("--members", "-m", nargs='+', help='char values specifying the types of the struct members')
parser.add_argument("--values", "-v", nargs='+', help='values of the struct members')
args = parser.parse_args()

#convert strings to python types
values_t, types_t = map(list, zip(*([conv_and_get_type(i) for i in (' '.join(args.values)).split()])))

#strings need to be converted to bytes for packing
for c, i in enumerate(values_t, start = 0):
    if types_t[c] == 2:
        values_t[c] = bytes(values_t[c], 'ascii')

topo_ctl_members = '2I'
topo_hdr_members = '5I'
topo_data_members = ''.join(args.members)
topo_total = topo_hdr_members + topo_data_members
topo_total_extra = topo_total + topo_ctl_members

#abi header struct values
magic = 0x00464F53
comp_type = 0
size = struct.calcsize(topo_total)
abi = args.abiver[0] << SOF_ABI_MAJOR_SHIFT | args.abiver[1] << SOF_ABI_MINOR_SHIFT | args.abiver[2] << SOF_ABI_PATCH_SHIFT
reserved = 0

size_extra = struct.calcsize(topo_total_extra)
#pack possible snd_ctl_tlv with controlid
if args.controlid:
    topo_hdr_extra = struct.pack(topo_ctl_members, args.controlid[0], size_extra)

#pack header
topo_hdr = struct.pack(topo_hdr_members, magic, comp_type, size, abi, reserved)

#pack payload
topo_data = struct.pack(topo_data_members, *(values_t))

#full blob

if args.controlid:
    topo_full = topo_hdr_extra + topo_hdr + topo_data
else:
    topo_full = topo_hdr + topo_data

#for sof topology m4
print("sof m4 and alsa conf format:")
print('`\tbytes \"', end='')

for i, b in enumerate(topo_full):
    print(''.join('0x{:02x}'.format(b)), end='')
    print(',', end='')
    if i % items_per_line == 0 and i != 0:
        print("'\n", end='')
        print('`\t', end='')

print("'\n")

#for sof ctl tool
if not args.controlid:
    print("sof ctl tool format:")
    print(struct.unpack(topo_total, topo_full))
