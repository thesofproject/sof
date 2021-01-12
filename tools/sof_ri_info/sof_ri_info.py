#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2019, Intel Corporation. All rights reserved.
#
# Author: Marcin Maka <marcin.maka@linux.intel.com>

""" Parses manifests included in sof binary and prints extracted metadata
    in readable form.
"""

# pylint: disable=too-few-public-methods
# pylint: disable=too-many-arguments
# pylint: disable=too-many-instance-attributes

import sys
import argparse
import struct

# To extend the DSP memory layout list scroll down to DSP_MEM_SPACE_EXT

# Public keys signatures recognized by parse_css_manifest()
# - add a new one as array of bytes and append entry to KNOWN_KEYS below.
#   you can use --full_bytes to dump the bytes in below format

APL_INTEL_PROD_KEY = bytes([0x1f, 0xf4, 0x58, 0x74, 0x64, 0xd4, 0xae, 0x90,
                            0x03, 0xb6, 0x71, 0x0d, 0xb5, 0xaf, 0x6d, 0xd6,
                            0x96, 0xce, 0x28, 0x95, 0xd1, 0x5b, 0x40, 0x59,
                            0xcd, 0xdf, 0x0c, 0x55, 0xd2, 0xc1, 0xbd, 0x58,
                            0xc3, 0x0d, 0x83, 0xe2, 0xac, 0xfa, 0xe0, 0xcc,
                            0x54, 0xf6, 0x5f, 0x72, 0xc2, 0x11, 0x05, 0x93,
                            0x1d, 0xb7, 0xe4, 0x4f, 0xa4, 0x95, 0xf5, 0x84,
                            0x77, 0x07, 0x24, 0x6e, 0x72, 0xce, 0x57, 0x41,
                            0xf2, 0x0b, 0x49, 0x49, 0x0c, 0xe2, 0x76, 0xf8,
                            0x19, 0xc7, 0x9f, 0xe1, 0xca, 0x77, 0x20, 0x1b,
                            0x5d, 0x1d, 0xed, 0xee, 0x5c, 0x54, 0x1d, 0xf6,
                            0x76, 0x14, 0xce, 0x6a, 0x24, 0x80, 0xc9, 0xce,
                            0x2e, 0x92, 0xe9, 0x35, 0xc7, 0x1a, 0xe9, 0x97,
                            0x7f, 0x25, 0x2b, 0xa8, 0xf3, 0xc1, 0x4d, 0x6b,
                            0xae, 0xd9, 0xcd, 0x0c, 0xbb, 0x08, 0x6d, 0x2b,
                            0x01, 0x44, 0xe2, 0xb9, 0x44, 0x4e, 0x4d, 0x5c,
                            0xdf, 0x8a, 0x89, 0xa5, 0x3c, 0x27, 0xa0, 0x54,
                            0xde, 0xc5, 0x5b, 0xde, 0x58, 0x10, 0x8c, 0xaa,
                            0xc4, 0x37, 0x5b, 0x73, 0x58, 0xfb, 0xe3, 0xcf,
                            0x57, 0xf5, 0x65, 0xd3, 0x19, 0x06, 0xed, 0x36,
                            0x47, 0xb0, 0x91, 0x67, 0xec, 0xc1, 0xe1, 0x7b,
                            0x4f, 0x85, 0x66, 0x61, 0x31, 0x99, 0xfc, 0x98,
                            0x7a, 0x56, 0x70, 0x95, 0x85, 0x52, 0xa0, 0x30,
                            0x37, 0x92, 0x11, 0x9e, 0x7f, 0x33, 0x44, 0xd3,
                            0x81, 0xfd, 0x14, 0x74, 0x51, 0x1c, 0x01, 0x14,
                            0xc8, 0x4b, 0xf6, 0xd6, 0xeb, 0x67, 0xef, 0xfc,
                            0x0a, 0x5f, 0xcc, 0x31, 0x73, 0xf8, 0xa9, 0xe3,
                            0xcb, 0xb4, 0x8b, 0x91, 0xa1, 0xf0, 0xb9, 0x6e,
                            0x1f, 0xea, 0xd3, 0xa3, 0xe4, 0x0f, 0x96, 0x74,
                            0x3c, 0x17, 0x5b, 0x68, 0x7c, 0x87, 0xfc, 0x90,
                            0x10, 0x89, 0x23, 0xca, 0x5d, 0x17, 0x5b, 0xc1,
                            0xb5, 0xc2, 0x49, 0x4e, 0x2a, 0x5f, 0x47, 0xc2])

CNL_INTEL_PROD_KEY = bytes([0x41, 0xa0, 0x3e, 0x14, 0x1e, 0x7e, 0x29, 0x72,
                            0x89, 0x97, 0xc2, 0xa7, 0x7d, 0xbc, 0x1d, 0x25,
                            0xf4, 0x9a, 0xa8, 0xb7, 0x89, 0x10, 0x73, 0x31,
                            0x58, 0xbd, 0x46, 0x55, 0x78, 0xcf, 0xd9, 0xe1,
                            0x7d, 0xfa, 0x24, 0x23, 0xfa, 0x5c, 0x7c, 0xc9,
                            0x3d, 0xc8, 0xb5, 0x74, 0x87, 0xa, 0x8c, 0xe7,
                            0x33, 0xc2, 0x71, 0x26, 0xb1, 0x4d, 0x32, 0x45,
                            0x23, 0x17, 0xcb, 0xa6, 0xa2, 0xd0, 0xcc, 0x9e,
                            0x2b, 0xa6, 0x9, 0x42, 0x52, 0xf1, 0xe6, 0xbd,
                            0x73, 0x92, 0x2a, 0xfb, 0x7f, 0xc4, 0x8d, 0x5,
                            0xec, 0x69, 0x7f, 0xd4, 0xa2, 0x6c, 0x46, 0xd4,
                            0x5d, 0x92, 0x1d, 0x17, 0x75, 0x39, 0x16, 0x4c,
                            0x61, 0xa8, 0xda, 0x93, 0xd6, 0x26, 0x23, 0xa,
                            0xc8, 0x2d, 0xcc, 0x81, 0xf4, 0xcc, 0x85, 0x42,
                            0xaa, 0xa3, 0x15, 0x8, 0x62, 0x8f, 0x72, 0x9b,
                            0x5f, 0x90, 0x2f, 0xd5, 0x94, 0xdc, 0xad, 0xf,
                            0xa9, 0x8, 0x8c, 0x2e, 0x20, 0xf4, 0xdf, 0x12,
                            0xf, 0xe2, 0x1e, 0xeb, 0xfb, 0xf7, 0xe9, 0x22,
                            0xef, 0xa7, 0x12, 0x3d, 0x43, 0x3b, 0x62, 0x8e,
                            0x2e, 0xeb, 0x78, 0x8, 0x6e, 0xd0, 0xb0, 0xea,
                            0x37, 0x43, 0x16, 0xd8, 0x11, 0x5a, 0xb5, 0x5,
                            0x60, 0xf2, 0x91, 0xa7, 0xaa, 0x7d, 0x7, 0x17,
                            0xb7, 0x5b, 0xec, 0x45, 0xf4, 0x4a, 0xaf, 0x5c,
                            0xa3, 0x30, 0x62, 0x8e, 0x4d, 0x63, 0x2, 0x2,
                            0xed, 0x4b, 0x1f, 0x1b, 0x9a, 0x2, 0x29, 0x9,
                            0xc1, 0x7a, 0xc5, 0xeb, 0xc7, 0xdb, 0xa1, 0x6f,
                            0x61, 0x31, 0xfa, 0x7b, 0x3b, 0xe0, 0x6a, 0x1c,
                            0xee, 0x55, 0xed, 0xf0, 0xf9, 0x7a, 0xaf, 0xaa,
                            0xc7, 0x76, 0xf5, 0xfb, 0x6a, 0xbc, 0x65, 0xde,
                            0x42, 0x3e, 0x1c, 0xdf, 0xcc, 0x69, 0x75, 0x1,
                            0x38, 0x8, 0x66, 0x20, 0xea, 0x6, 0x91, 0xb8,
                            0xcd, 0x1d, 0xfa, 0xfd, 0xe8, 0xa0, 0xba, 0x91])

ICL_INTEL_PROD_KEY = bytes([0x63, 0xdf, 0x54, 0xe3, 0xc1, 0xe5, 0xd9, 0xd2,
                            0xb8, 0xb5, 0x13, 0xb3, 0xec, 0xc2, 0x64, 0xb5,
                            0x16, 0xb4, 0xfc, 0x56, 0x92, 0x67, 0x17, 0xc7,
                            0x91, 0x7b, 0x3d, 0xb0, 0x22, 0xbf, 0x7f, 0x92,
                            0x39, 0x35, 0xcc, 0x64, 0x1c, 0xad, 0x8, 0x75,
                            0xe7, 0x67, 0xb, 0x8, 0xf8, 0x57, 0xdb, 0x9c,
                            0xde, 0xab, 0xe, 0xbd, 0x27, 0x5f, 0x5, 0x51,
                            0xcf, 0x6e, 0x3e, 0xc9, 0xdd, 0xe6, 0x51, 0x14,
                            0x57, 0xe1, 0x8a, 0x23, 0xae, 0x7a, 0xa5, 0x5f,
                            0xdc, 0x16, 0x13, 0x1b, 0x28, 0x3b, 0xab, 0xf1,
                            0xc3, 0xb5, 0x73, 0xc0, 0x72, 0xd8, 0x86, 0x7a,
                            0x76, 0x3a, 0x2, 0xbe, 0x2f, 0x3e, 0xfe, 0x93,
                            0x83, 0xa1, 0xd, 0xa0, 0xfc, 0x26, 0x7f, 0x6b,
                            0x2e, 0x5a, 0xfd, 0xac, 0x6b, 0x53, 0xd3, 0xb8,
                            0xff, 0x5e, 0xc7, 0x5, 0x25, 0xff, 0xe7, 0x78,
                            0x9c, 0x45, 0xe4, 0x17, 0xbd, 0xf4, 0x52, 0x4e,
                            0x3c, 0xa2, 0xa, 0x4d, 0x54, 0xb5, 0x40, 0x30,
                            0xb3, 0x48, 0xba, 0x6c, 0xfa, 0x63, 0xc0, 0x65,
                            0x2e, 0xde, 0x9, 0x2e, 0xa1, 0x95, 0x85, 0xc0,
                            0x78, 0xd9, 0x98, 0x64, 0x3c, 0x29, 0x2e, 0x48,
                            0x66, 0x1e, 0xaf, 0x1d, 0xa0, 0x7c, 0x15, 0x3,
                            0x7f, 0x9e, 0x5f, 0x38, 0xf5, 0xc1, 0xe1, 0xe9,
                            0xbe, 0x77, 0xa2, 0x9c, 0x83, 0xf2, 0x25, 0x54,
                            0x22, 0xfe, 0x29, 0x66, 0x5, 0xc2, 0xc9, 0x6b,
                            0x8b, 0xa6, 0xa3, 0xf9, 0xb1, 0x6b, 0xaf, 0xe7,
                            0x14, 0x77, 0xff, 0x17, 0xc9, 0x7c, 0x7c, 0x4e,
                            0x83, 0x28, 0x2a, 0xe5, 0xc3, 0xcc, 0x6e, 0x25,
                            0xa, 0x62, 0xbb, 0x97, 0x44, 0x86, 0x7c, 0xa2,
                            0xd4, 0xf1, 0xd4, 0xf8, 0x8, 0x17, 0xf4, 0x6c,
                            0xcc, 0x95, 0x99, 0xd4, 0x86, 0x37, 0x4, 0x9f,
                            0x5, 0x76, 0x1b, 0x44, 0x55, 0x75, 0xd9, 0x32,
                            0x35, 0xf1, 0xec, 0x4d, 0x93, 0x73, 0xe6, 0xc4])

TGL_INTEL_PROD_KEY = bytes([0xd3, 0x72, 0x92, 0x99, 0x4e, 0xb9, 0xcd, 0x67,
                            0x41, 0x86, 0x16, 0x77, 0x35, 0xa1, 0x34, 0x85,
                            0x43, 0x96, 0xd9, 0x53, 0x76, 0x4d, 0xd0, 0x63,
                            0x17, 0x72, 0x96, 0xee, 0xf6, 0xdc, 0x50, 0x53,
                            0x4b, 0x4, 0xaa, 0xfe, 0x3d, 0xd7, 0x21, 0x29,
                            0x79, 0x6, 0x76, 0xee, 0xb3, 0x70, 0x23, 0x8,
                            0x26, 0xa8, 0x83, 0x3d, 0x70, 0x13, 0x9d, 0x65,
                            0xcb, 0xd5, 0xc6, 0xf, 0x92, 0x93, 0x38, 0x29,
                            0x19, 0xa6, 0x7c, 0xbf, 0xf1, 0x76, 0x75, 0x2,
                            0x9e, 0x32, 0x8f, 0x1f, 0x5, 0xa6, 0x2d, 0x89,
                            0x6d, 0x38, 0xba, 0x38, 0xd, 0xf1, 0xe9, 0xe8,
                            0xed, 0xf7, 0x6c, 0x20, 0x8d, 0x91, 0xc, 0xf8,
                            0xdd, 0x9a, 0x56, 0xd3, 0xf7, 0xbf, 0x3c, 0xda,
                            0xc8, 0x5d, 0xb, 0xef, 0x20, 0x5a, 0xc1, 0x5f,
                            0x91, 0x94, 0xee, 0x90, 0xb8, 0xfc, 0x2c, 0x31,
                            0x75, 0xc3, 0x7e, 0x86, 0xf6, 0x4f, 0x45, 0x4c,
                            0x64, 0xe1, 0xe9, 0xe5, 0xcd, 0xf0, 0xec, 0xef,
                            0xa7, 0xbd, 0x31, 0x62, 0x40, 0xa8, 0x48, 0x52,
                            0xd5, 0x23, 0xce, 0x4, 0x45, 0x2f, 0xb, 0x3d,
                            0xe0, 0x7a, 0xcf, 0xe5, 0x2a, 0x45, 0x5e, 0x91,
                            0x1d, 0x41, 0xa7, 0x40, 0x85, 0x34, 0xe, 0x50,
                            0x45, 0x59, 0xbf, 0xd, 0xa6, 0x6, 0xf9, 0xf6,
                            0xce, 0xa2, 0x76, 0x72, 0x0, 0x62, 0x73, 0x37,
                            0x1a, 0xbe, 0xd2, 0xe3, 0x1b, 0x7b, 0x26, 0x7b,
                            0x32, 0xaa, 0x79, 0xed, 0x59, 0x23, 0xb6, 0xdb,
                            0x9f, 0x3c, 0x3d, 0x65, 0xf3, 0xbb, 0x4b, 0xb4,
                            0x97, 0xaa, 0x2a, 0xae, 0x48, 0xf4, 0xc5, 0x59,
                            0x8d, 0x82, 0x4a, 0xb, 0x15, 0x4d, 0xd5, 0x4,
                            0xa6, 0xc1, 0x2d, 0x83, 0x19, 0xc4, 0xc6, 0x49,
                            0xba, 0x0, 0x1b, 0x2b, 0x70, 0xb, 0x26, 0x7c,
                            0xb8, 0x94, 0x18, 0xe4, 0x9a, 0xf6, 0x5a, 0x68,
                            0x9d, 0x44, 0xd2, 0xed, 0xd5, 0x67, 0x42, 0x47,
                            0x5f, 0x73, 0xc5, 0xa7, 0xe5, 0x87, 0xa9, 0x4d,
                            0xae, 0xc1, 0xb, 0x2c, 0x46, 0x16, 0xd7, 0x4e,
                            0xf0, 0xdc, 0x61, 0x58, 0x51, 0xb1, 0x2, 0xbc,
                            0xca, 0x17, 0xb1, 0x1a, 0xa, 0x96, 0x3b, 0x25,
                            0x1c, 0x63, 0x56, 0x65, 0x20, 0x6e, 0x1b, 0x21,
                            0xb1, 0x94, 0x7a, 0xf5, 0xbf, 0x83, 0x21, 0x86,
                            0x38, 0xf1, 0x66, 0x1a, 0xa, 0x75, 0x73, 0xa,
                            0xe, 0xc7, 0x64, 0x68, 0xc7, 0xf9, 0xc3, 0x4a,
                            0x73, 0xfb, 0x86, 0xa5, 0x7, 0xb8, 0x8b, 0xf0,
                            0xa3, 0x3b, 0xa9, 0x8f, 0x33, 0xa7, 0xce, 0xfe,
                            0x36, 0x60, 0xbd, 0x5, 0xf0, 0x9a, 0x30, 0xe5,
                            0xe1, 0x43, 0x25, 0x1c, 0x1, 0x4a, 0xd4, 0x23,
                            0x1e, 0x8f, 0xb9, 0xdd, 0xd8, 0xb2, 0x24, 0xef,
                            0x36, 0x4d, 0x5b, 0x8f, 0xba, 0x4f, 0xe9, 0x48,
                            0xe7, 0x51, 0x42, 0x59, 0x56, 0xa, 0x1c, 0xf,
                            0x5d, 0x62, 0x4a, 0x80, 0x96, 0x31, 0xf8, 0xb5])

EHL_INTEL_PROD_KEY = bytes([0xb5, 0xb0, 0xe2, 0x25, 0x3d, 0xc7, 0x54, 0x10,
                            0xde, 0x3c, 0xc9, 0x24, 0x97, 0x74, 0xbc, 0x02,
                            0x7d, 0x0b, 0xd6, 0x61, 0x2e, 0x35, 0x65, 0xed,
                            0x78, 0xf6, 0x85, 0x73, 0x1f, 0x8c, 0xda, 0x8f,
                            0x50, 0x79, 0xc7, 0x0c, 0x9e, 0xb4, 0x09, 0x3b,
                            0xfc, 0x2e, 0x4e, 0xf3, 0x46, 0xfe, 0x3f, 0x20,
                            0x9d, 0x8d, 0xf6, 0x3e, 0xc3, 0x46, 0x92, 0xf9,
                            0xce, 0xbb, 0x7d, 0x0b, 0xb3, 0x45, 0x35, 0x76,
                            0xbe, 0x19, 0x87, 0x21, 0x6c, 0x79, 0xfa, 0xf4,
                            0xc8, 0x8e, 0x07, 0x26, 0x03, 0x0d, 0xe9, 0xe3,
                            0x1e, 0x61, 0x7c, 0xd1, 0x45, 0x10, 0x61, 0x1c,
                            0x79, 0x3f, 0x10, 0xa9, 0x42, 0x60, 0x2c, 0x7a,
                            0x7a, 0x89, 0x1b, 0x54, 0xda, 0x0e, 0x54, 0x08,
                            0x30, 0x0f, 0x6e, 0x37, 0xea, 0xb7, 0x58, 0xa0,
                            0xaf, 0x4a, 0x94, 0x2c, 0x43, 0x50, 0x74, 0xed,
                            0x16, 0xdc, 0x11, 0xa1, 0xd3, 0x6e, 0x54, 0xa6,
                            0x56, 0xf9, 0x40, 0x8c, 0x3f, 0xa3, 0x74, 0xae,
                            0x4f, 0x48, 0xc8, 0x79, 0x30, 0x5a, 0x99, 0x79,
                            0x26, 0xe1, 0x52, 0x9b, 0xfe, 0x9e, 0xaf, 0x96,
                            0xcc, 0xe6, 0x9a, 0x53, 0x2e, 0xe4, 0x40, 0xcc,
                            0xad, 0x19, 0x8e, 0x23, 0x53, 0x63, 0xc8, 0xfd,
                            0x96, 0xeb, 0x27, 0x9b, 0x3e, 0x49, 0x0d, 0x90,
                            0xb0, 0x67, 0xb4, 0x05, 0x4a, 0x55, 0x5b, 0xb0,
                            0xa5, 0x68, 0xb8, 0x60, 0xa4, 0x81, 0x6a, 0x3e,
                            0x8c, 0xbc, 0x29, 0xcd, 0x85, 0x45, 0x3c, 0xf4,
                            0x86, 0xf8, 0x9b, 0x69, 0xb5, 0xc5, 0xb9, 0xaa,
                            0xc8, 0xed, 0x7d, 0x70, 0x45, 0xb6, 0xf6, 0x5b,
                            0x48, 0x62, 0xf6, 0x68, 0xe8, 0xdd, 0x79, 0xda,
                            0xb0, 0xe9, 0x3c, 0x8f, 0x01, 0x92, 0x80, 0x73,
                            0x89, 0x7d, 0x9a, 0xaf, 0x31, 0x85, 0x75, 0x7c,
                            0x89, 0xf3, 0x6c, 0x77, 0x95, 0x5b, 0xa9, 0xc5,
                            0xe1, 0x33, 0xe0, 0x44, 0x81, 0x7e, 0x72, 0xa5,
                            0xbb, 0x3d, 0x40, 0xb7, 0xc9, 0x77, 0xd8, 0xc3,
                            0xe3, 0xef, 0x42, 0xae, 0x57, 0x91, 0x63, 0x0c,
                            0x26, 0xac, 0x5e, 0x10, 0x51, 0x28, 0xe6, 0x61,
                            0xad, 0x4d, 0xc4, 0x93, 0xb2, 0xe0, 0xb4, 0x31,
                            0x60, 0x5a, 0x97, 0x0e, 0x80, 0x86, 0x91, 0xc9,
                            0xcd, 0xfc, 0x97, 0xc3, 0x78, 0xbd, 0xca, 0xce,
                            0xd3, 0x96, 0xee, 0x75, 0x81, 0xe0, 0x8b, 0x45,
                            0x8e, 0x20, 0x4b, 0x98, 0x31, 0x0f, 0xf9, 0x66,
                            0xb3, 0x04, 0xb7, 0x0d, 0xde, 0x68, 0x1e, 0x2a,
                            0xe4, 0xec, 0x45, 0x2a, 0x0a, 0x24, 0x81, 0x82,
                            0xcb, 0x86, 0xa0, 0x61, 0x7f, 0xe7, 0x96, 0x84,
                            0x4b, 0x30, 0xc4, 0x7d, 0x5c, 0x1b, 0x2c, 0x1e,
                            0x66, 0x68, 0x71, 0x1d, 0x39, 0x6c, 0x23, 0x07,
                            0x6d, 0xf3, 0x3e, 0x64, 0xc3, 0x03, 0x97, 0x84,
                            0x14, 0xd1, 0xf6, 0x50, 0xf4, 0x32, 0x5d, 0xae,
                            0xad, 0x23, 0x46, 0x0c, 0x9f, 0xfc, 0x3e, 0xb9])

COMMUNITY_KEY = bytes([0x85, 0x00, 0xe1, 0x68, 0xaa, 0xeb, 0xd2, 0x07,
                       0x1b, 0x7c, 0x5e, 0xed, 0xd6, 0xe7, 0xe5, 0xf9,
                       0xc1, 0x0e, 0x47, 0xd4, 0x4c, 0xab, 0x8c, 0xf0,
                       0xe8, 0xee, 0x8b, 0x40, 0x36, 0x35, 0x58, 0x8f,
                       0xf4, 0x6f, 0xfc, 0xfd, 0x0f, 0xdd, 0x55, 0x8b,
                       0x45, 0x8c, 0xf0, 0x47, 0xdc, 0xb4, 0xac, 0x21,
                       0x3b, 0x4b, 0x20, 0xe6, 0x81, 0xb3, 0xcc, 0x90,
                       0xd4, 0x5e, 0xf1, 0xa4, 0x9b, 0x68, 0x52, 0xc8,
                       0xf1, 0x2d, 0xf9, 0xc4, 0x77, 0xc6, 0x4d, 0xa9,
                       0x90, 0xc7, 0x10, 0xfd, 0x43, 0xc8, 0x4b, 0x6b,
                       0x23, 0x5e, 0x92, 0xf5, 0x8f, 0xac, 0xd5, 0x7d,
                       0x60, 0x27, 0x36, 0x7c, 0x21, 0x4e, 0x21, 0x99,
                       0xde, 0xcb, 0xc0, 0x45, 0xf3, 0x04, 0x22, 0xb8,
                       0x7d, 0x16, 0x68, 0x40, 0xf9, 0x5c, 0xf0, 0xb9,
                       0x7e, 0x8c, 0x05, 0xb6, 0xfc, 0x28, 0xbb, 0x3d,
                       0xd8, 0xff, 0xb6, 0xa4, 0xd4, 0x54, 0x27, 0x3b,
                       0x1a, 0x42, 0x4e, 0xf5, 0xa6, 0xa8, 0x5e, 0x44,
                       0xe2, 0x9e, 0xed, 0x68, 0x6a, 0x27, 0x60, 0x13,
                       0x8d, 0x2f, 0x27, 0x70, 0xcd, 0x57, 0xc9, 0x18,
                       0xa3, 0xb0, 0x30, 0xa1, 0xf4, 0xe6, 0x32, 0x12,
                       0x89, 0x2a, 0xaf, 0x40, 0xa5, 0xfd, 0x52, 0xf1,
                       0xaa, 0x8a, 0xa4, 0xef, 0x20, 0x3d, 0x10, 0xa3,
                       0x70, 0xf2, 0x39, 0xc5, 0x05, 0x99, 0x22, 0x10,
                       0x81, 0x83, 0x6e, 0x45, 0xa4, 0xf3, 0x5a, 0x9d,
                       0x6a, 0xb8, 0x88, 0xfe, 0x69, 0x40, 0xd1, 0xb1,
                       0xcb, 0x2a, 0xdb, 0x28, 0x05, 0xde, 0x54, 0xbf,
                       0x3d, 0x86, 0x5f, 0x39, 0x8b, 0xc1, 0xf4, 0xaf,
                       0x00, 0x61, 0x86, 0x01, 0xfa, 0x22, 0xac, 0xf6,
                       0x2c, 0xa4, 0x17, 0x6a, 0xa7, 0xd8, 0x0a, 0x8c,
                       0x9f, 0xbf, 0x1f, 0x62, 0xb2, 0x2e, 0x68, 0x52,
                       0x3f, 0x82, 0x8f, 0xe5, 0x28, 0x4d, 0xdb, 0xb5,
                       0x5a, 0x96, 0x28, 0x27, 0x19, 0xaf, 0x43, 0xb9])

COMMUNITY_KEY2 = bytes([0x6b, 0x75, 0xed, 0x58, 0x20, 0x08, 0x85, 0x95,
                        0xa0, 0x49, 0x8b, 0x9e, 0xbd, 0x5f, 0x34, 0x82,
                        0x0a, 0x9d, 0x1e, 0x9a, 0xb6, 0x76, 0x43, 0x19,
                        0xb7, 0x76, 0x45, 0x5b, 0x59, 0xab, 0xbb, 0xf3,
                        0x9e, 0x72, 0xf2, 0x41, 0x24, 0x92, 0x97, 0xef,
                        0x39, 0xc0, 0xed, 0xc4, 0x7a, 0x4e, 0xdb, 0xec,
                        0xeb, 0xc7, 0x4c, 0xf6, 0x45, 0xbe, 0xb2, 0xe0,
                        0x13, 0x6a, 0xdc, 0x06, 0x7a, 0x1c, 0xbd, 0x8d,
                        0xd8, 0xd2, 0xd7, 0x82, 0x6d, 0xbe, 0x03, 0x76,
                        0x3b, 0x6b, 0xb8, 0x2f, 0xcc, 0xbe, 0x30, 0x56,
                        0x61, 0x87, 0x09, 0xdf, 0x44, 0x51, 0xf8, 0x82,
                        0xc5, 0x78, 0x05, 0x45, 0x8c, 0xe3, 0x78, 0x0e,
                        0xd3, 0x7a, 0xd4, 0xf4, 0xbe, 0x96, 0xde, 0xb8,
                        0x3b, 0x78, 0x90, 0x8b, 0xd3, 0xdd, 0x0b, 0xdd,
                        0xbe, 0x56, 0xf3, 0x9a, 0x34, 0xc9, 0x38, 0x47,
                        0x8d, 0xc4, 0xbd, 0x5e, 0x68, 0xf8, 0x62, 0xc4,
                        0x28, 0xdd, 0x00, 0x48, 0x93, 0xb5, 0xad, 0x74,
                        0x52, 0xe5, 0xf3, 0xd2, 0x97, 0xde, 0xbc, 0x0a,
                        0x85, 0x95, 0xe9, 0xfa, 0xd2, 0xac, 0xdc, 0xdc,
                        0x59, 0x74, 0xfa, 0x57, 0xf2, 0xd3, 0x61, 0xc6,
                        0x2b, 0x26, 0xde, 0x57, 0x50, 0xe2, 0x58, 0x6b,
                        0x79, 0x65, 0x0b, 0x49, 0x2c, 0x59, 0x28, 0x25,
                        0x64, 0x31, 0x93, 0x65, 0x9a, 0x0a, 0x88, 0x98,
                        0x9a, 0x77, 0x00, 0x47, 0x8f, 0xa0, 0xc7, 0x6b,
                        0x58, 0x90, 0xa9, 0xb5, 0x15, 0xff, 0x65, 0x7c,
                        0x84, 0x02, 0xd4, 0xdd, 0x09, 0xf1, 0x25, 0xad,
                        0xf9, 0x30, 0xaa, 0x34, 0x5b, 0x77, 0xef, 0xb2,
                        0x75, 0x3d, 0x54, 0x9d, 0xcc, 0x0d, 0x11, 0xda,
                        0x91, 0x01, 0x2e, 0x51, 0xdc, 0x0c, 0x8a, 0x92,
                        0x71, 0x44, 0x9a, 0xd5, 0x69, 0x5d, 0x7a, 0xad,
                        0xaf, 0xdf, 0x25, 0xea, 0x95, 0x21, 0xbb, 0x99,
                        0x53, 0x89, 0xbc, 0x54, 0xca, 0xf3, 0x54, 0xf5,
                        0xbb, 0x38, 0x27, 0x64, 0xce, 0xf2, 0x17, 0x25,
                        0x75, 0x33, 0x1a, 0x2d, 0x19, 0x00, 0xff, 0x9b,
                        0xd9, 0x4d, 0x0c, 0xb1, 0xa5, 0x55, 0x55, 0xa9,
                        0x29, 0x8e, 0xfb, 0x82, 0x43, 0xeb, 0xfa, 0xc8,
                        0x33, 0x76, 0xf3, 0x7d, 0xee, 0x95, 0xe1, 0x39,
                        0xba, 0xa5, 0x4a, 0xd5, 0xb1, 0x8a, 0xa6, 0xff,
                        0x8f, 0x4b, 0x45, 0x8c, 0xe9, 0x7b, 0x87, 0xae,
                        0x8d, 0x32, 0x6e, 0x16, 0xe7, 0x9e, 0x85, 0x22,
                        0x71, 0x3d, 0x17, 0xba, 0x54, 0xed, 0x73, 0x87,
                        0xe5, 0x9d, 0xbf, 0xc0, 0xcd, 0x76, 0xfa, 0x83,
                        0xd4, 0xc5, 0x30, 0xd1, 0xc7, 0x25, 0x49, 0x25,
                        0x75, 0x4d, 0x0a, 0x4a, 0x2d, 0x13, 0x1c, 0x12,
                        0x2e, 0x5d, 0x2a, 0xe2, 0xa9, 0xae, 0xbf, 0x8f,
                        0xdf, 0x24, 0x76, 0xf5, 0x81, 0x1e, 0x09, 0x5d,
                        0x63, 0x04, 0xaf, 0x24, 0x45, 0x87, 0xf4, 0x96,
                        0x55, 0xd1, 0x7d, 0xc6, 0x0d, 0x79, 0x12, 0xa9])

KNOWN_KEYS = {APL_INTEL_PROD_KEY : 'APL Intel prod key',
              CNL_INTEL_PROD_KEY : 'CNL Intel prod key',
              ICL_INTEL_PROD_KEY : 'ICL Intel prod key',
              TGL_INTEL_PROD_KEY : 'TGL Intel prod key',
              EHL_INTEL_PROD_KEY : 'EHL Intel prod key',
              COMMUNITY_KEY : 'Community key',
              COMMUNITY_KEY2 : 'Community 3k key'}

def parse_params():
    """ Parses parameters
    """
    parser = argparse.ArgumentParser(description='SOF Binary Info Utility')
    parser.add_argument('-v', '--verbose', help='increase output verbosity',
                        action='store_true')
    parser.add_argument('--headers', help='display headers only',
                        action='store_true')
    parser.add_argument('--full_bytes', help='display full byte arrays',
                        action='store_true')
    parser.add_argument('--no_colors', help='disable colors in output',
                        action='store_true')
    parser.add_argument('--no_cse', help='disable cse manifest parsing',
                        action='store_true')
    parser.add_argument('--no_headers', help='skip information about headers',
                        action='store_true')
    parser.add_argument('--no_modules', help='skip information about modules',
                        action='store_true')
    parser.add_argument('--no_memory', help='skip information about memory',
                        action='store_true')
    parser.add_argument('sof_ri_path', help='path to fw binary file to parse')
    parsed_args = parser.parse_args()

    return parsed_args

# Helper Functions

def change_color(color):
    """ Prints escape code to change text color
    """
    color_code = {'red':91, 'green':92, 'yellow':93, 'blue':94,
                  'magenta':95, 'cyan':96, 'white':98, 'none':0}
    return '\033[{}m'.format(color_code[color])

def uint_to_string(uint, both=False):
    """ Prints uint in readable form
    """
    if both:
        return hex(uint) + ' (' + repr(uint) + ')'
    return hex(uint)

def date_to_string(date):
    """ Prints BCD date in readable form
    """
    return date[2:6]+'/'+date[6:8]+'/'+date[8:10]

def chararr_to_string(chararr, max_len):
    """ Prints array of characters (null terminated or till max_len)
        in readable form
    """
    out = ''
    for i in range(0, max_len):
        if chararr[i] == 0:
            return out
        out += '{:c}'.format(chararr[i])
    return out

def mod_type_to_string(mod_type):
    """ Prints module type in readable form
    """
    out = '('
    # type
    if (mod_type & 0xf) == 0:
        out += ' builtin'
    elif (mod_type & 0xf) == 1:
        out += ' loadable'
    # Module that may be instantiated by fw on startup
    if ((mod_type >> 4) & 0x1) == 1:
        out += ' auto_start'
    # Module designed to run with low latency scheduler
    if ((mod_type >> 5) & 0x1) == 1:
        out += ' LL'
    # Module designed to run with edf scheduler
    if ((mod_type >> 6) & 0x1) == 1:
        out += ' DP'
    out += ' )'
    return out

def seg_flags_to_string(flags):
    """ Prints module segment flags in readable form
    """
    out = '('
    if flags & 0x1 == 0x1:
        out = out + ' contents'
    if flags & 0x2 == 0x2:
        out = out + ' alloc'
    if flags & 0x4 == 0x4:
        out = out + ' load'
    if flags & 0x8 == 0x8:
        out = out + ' readonly'
    if flags & 0x10 == 0x10:
        out = out + ' code'
    if flags & 0x20 == 0x20:
        out = out + ' data'
    out = out + ' type=' + repr((flags>>8)&0xf)
    out = out + ' pages=' + repr((flags>>16)&0xffff)
    out = out + ' )'
    return out

# Parsers

def parse_extended_manifest_ae1(reader):
    ext_mft = ExtendedManifestAE1()
    hdr = Component('ext_mft_hdr', 'Header', 0)
    ext_mft.add_comp(hdr)

    sig = reader.read_string(4)
    reader.info('Extended Manifest (' + sig + ')', -4)
    hdr.add_a(Astring('sig', sig))

    # Next dword is the total length of the extended manifest
    # (need to use it for further parsing)
    reader.ext_mft_length = reader.read_dw()
    hdr.add_a(Auint('length', reader.ext_mft_length))
    hdr.add_a(Astring('ver', '{}.{}'.format(reader.read_w(), reader.read_w())))
    hdr.add_a(Auint('entries', reader.read_dw()))

    reader.ff_data(reader.ext_mft_length-16)
    return ext_mft

def parse_extended_manifest_xman(reader):
    ext_mft = ExtendedManifestXMan()
    hdr = Component('ext_mft_hdr', 'Header', 0)
    ext_mft.add_comp(hdr)

    sig = reader.read_string(4)
    reader.info('Extended Manifest (' + sig + ')', -4)
    hdr.add_a(Astring('sig', sig))

    # Next dword is the total length of the extended manifest
    # (need to use it for further parsing)
    reader.ext_mft_length = reader.read_dw()
    hdr_length = reader.read_dw()
    hdr_ver = reader.read_dw()

    major = hdr_ver >> 24
    minor = (hdr_ver >> 12) & 0xFFF
    patch = hdr_ver & 0xFFF

    hdr.add_a(Auint('length', reader.ext_mft_length))
    hdr.add_a(Astring('ver', '{}.{}.{}'.format(major, minor, patch)))
    hdr.add_a(Auint('hdr_length', hdr_length))

    reader.ff_data(reader.ext_mft_length-16)
    return ext_mft

def parse_extended_manifest(reader):
    """ Parses extended manifest from sof binary
    """

    reader.info('Looking for Extended Manifest')
    # Try to detect signature first
    sig = reader.read_string(4)
    reader.set_offset(0)
    if sig == '$AE1':
        ext_mft = parse_extended_manifest_ae1(reader)
    elif sig == 'XMan':
        ext_mft = parse_extended_manifest_xman(reader)
    else:
        ext_mft = ExtendedManifestAE1()
        hdr = Component('ext_mft_hdr', 'Header', 0)
        ext_mft.add_comp(hdr)
        reader.info('info: Extended Manifest not found (sig = '+sig+')')
        reader.ext_mft_length = 0
        hdr.add_a(Auint('length', reader.ext_mft_length))

    return ext_mft

def parse_cse_manifest(reader):
    """ Parses CSE manifest form sof binary
    """
    reader.info('Looking for CSE Manifest')
    cse_mft = CseManifest(reader.get_offset())

    # Try to detect signature first
    sig = reader.read_string(4)
    if sig != '$CPD':
        reader.error('CSE Manifest NOT found ' + sig + ')', -4)
        sys.exit(1)
    reader.info('CSE Manifest (' + sig + ')', -4)

    # Read the header
    hdr = Component('cse_mft_hdr', 'Header', reader.get_offset())
    cse_mft.add_comp(hdr)
    hdr.add_a(Astring('sig', sig))
    # read number of entries
    nb_entries = reader.read_dw()
    reader.info('# of entries {}'.format(nb_entries))
    hdr.add_a(Adec('nb_entries', nb_entries))
    # read version (1byte for header ver and 1 byte for entry ver)
    ver = reader.read_w()
    hdr.add_a(Ahex('header_version', ver))
    header_length = reader.read_b()
    hdr.add_a(Ahex('header_length', header_length))
    hdr.add_a(Ahex('checksum', reader.read_b()))
    hdr.add_a(Astring('partition_name', reader.read_string(4)))

    reader.set_offset(cse_mft.file_offset + header_length)
    # Read entries
    nb_index = 0
    while nb_index < nb_entries:
        reader.info('Looking for CSE Manifest entry')
        entry_name = reader.read_string(12)
        entry_offset = reader.read_dw()
        entry_length = reader.read_dw()
        # reserved field
        reader.read_dw()

        hdr_entry = Component('cse_hdr_entry', 'Entry', reader.get_offset())
        hdr_entry.add_a(Astring('entry_name', entry_name))
        hdr_entry.add_a(Ahex('entry_offset', entry_offset))
        hdr_entry.add_a(Ahex('entry_length', entry_length))
        hdr.add_comp(hdr_entry)

        reader.info('CSE Entry name {} length {}'.format(entry_name,
                    entry_length))

        if '.man' in entry_name:
            entry = CssManifest(entry_name,
                                reader.ext_mft_length + entry_offset)
            cur_off = reader.set_offset(reader.ext_mft_length + entry_offset)
            parse_css_manifest(entry, reader,
                               reader.ext_mft_length + entry_offset + entry_length)
            reader.set_offset(cur_off)
        elif '.met' in entry_name:
            cur_off = reader.set_offset(reader.ext_mft_length + entry_offset)
            entry = parse_mft_extension(reader, 0)
            entry.name = '{} ({})'.format(entry_name, entry.name)
            reader.set_offset(cur_off)
        else:
            # indicate the place, the entry is enumerated. mft parsed later
            entry = Component('adsp_mft_cse_entry', entry_name, entry_offset)
        cse_mft.add_comp(entry)

        nb_index += 1

    return cse_mft

def parse_css_manifest(css_mft, reader, limit):
    """ Parses CSS manifest from sof binary
    """
    reader.info('Parsing CSS Manifest')
    ver, = struct.unpack('I', reader.get_data(0, 4))
    if ver == 4:
        reader.info('CSS Manifest type 4')
        return parse_css_manifest_4(css_mft, reader, limit)

    reader.error('CSS Manifest NOT found or NOT recognized!')
    sys.exit(1)

def parse_css_manifest_4(css_mft, reader, size_limit):
    """ Parses CSS manifest type 4 from sof binary
    """

    reader.info('Parsing CSS Manifest type 4')
    # CSS Header
    hdr = Component('css_mft_hdr', 'Header', reader.get_offset())
    css_mft.add_comp(hdr)

    hdr.add_a(Auint('type', reader.read_dw()))
    header_len_dw = reader.read_dw()
    hdr.add_a(Auint('header_len_dw', header_len_dw))
    hdr.add_a(Auint('header_version', reader.read_dw()))
    hdr.add_a(Auint('reserved0', reader.read_dw(), 'red'))
    hdr.add_a(Ahex('mod_vendor', reader.read_dw()))
    hdr.add_a(Adate('date', hex(reader.read_dw())))
    size = reader.read_dw()
    hdr.add_a(Auint('size', size))
    hdr.add_a(Astring('header_id', reader.read_string(4)))
    hdr.add_a(Auint('padding', reader.read_dw()))
    hdr.add_a(Aversion('fw_version', reader.read_w(), reader.read_w(),
                       reader.read_w(), reader.read_w()))
    hdr.add_a(Auint('svn', reader.read_dw()))
    reader.read_bytes(18*4)
    modulus_size = reader.read_dw()
    hdr.add_a(Adec('modulus_size', modulus_size))
    exponent_size = reader.read_dw()
    hdr.add_a(Adec('exponent_size', exponent_size))
    modulus = reader.read_bytes(modulus_size * 4)
    hdr.add_a(Amodulus('modulus', modulus, KNOWN_KEYS.get(modulus, 'Other')))
    hdr.add_a(Abytes('exponent', reader.read_bytes(exponent_size * 4)))
    hdr.add_a(Abytes('signature', reader.read_bytes(modulus_size * 4)))

    # Move right after the header
    reader.set_offset(css_mft.file_offset + header_len_dw*4)

    # Anything packed here is
    #   either an 'Extension' beginning with
    #     dw0 - extension type
    #     dw1 - extension length (in bytes)
    #   that could be parsed if extension type is recognized
    #
    #   or series of 0xffffffff that should be skipped
    reader.info('Parsing CSS Manifest extensions end 0x{:x}'.format(size_limit))
    ext_idx = 0
    while reader.get_offset() < size_limit:
        ext_type = reader.read_dw()
        reader.info('Reading extension type 0x{:x}'.format(ext_type))
        if ext_type == 0xffffffff:
            continue
        reader.set_offset(reader.get_offset() - 4)
        css_mft.add_comp(parse_mft_extension(reader, ext_idx))
        ext_idx += 1

    return css_mft

def parse_mft_extension(reader, ext_id):
    """ Parses mft extension from sof binary
    """
    ext_type = reader.read_dw()
    ext_len = reader.read_dw()
    if ext_type == 15:
        begin_off = reader.get_offset()
        ext = PlatFwAuthExtension(ext_id, reader.get_offset()-8)
        ext.add_a(Astring('name', reader.read_string(4)))
        ext.add_a(Auint('vcn', reader.read_dw()))
        ext.add_a(Abytes('bitmap', reader.read_bytes(16), 'red'))
        ext.add_a(Auint('svn', reader.read_dw()))
        read_len = reader.get_offset() - begin_off
        reader.ff_data(ext_len - read_len)
    elif ext_type == 17:
        ext = AdspMetadataFileExt(ext_id, reader.get_offset()-8)
        ext.add_a(Auint('adsp_imr_type', reader.read_dw(), 'red'))
        # skip reserved part
        reader.read_bytes(16)
        reader.read_dw()
        reader.read_dw()
        #
        ext.add_a(Auint('version', reader.read_dw()))
        ext.add_a(Abytes('sha_hash', reader.read_bytes(32)))
        ext.add_a(Auint('base_offset', reader.read_dw()))
        ext.add_a(Auint('limit_offset', reader.read_dw()))
        ext.add_a(Abytes('attributes', reader.read_bytes(16)))
    else:
        ext = MftExtension(ext_id, 'Other Extension', reader.get_offset()-8)
        reader.ff_data(ext_len-8)
    ext.add_a(Auint('type', ext_type))
    ext.add_a(Auint('length', ext_len))
    return ext

def parse_adsp_manifest_hdr(reader):
    """ Parses ADSP manifest hader from sof binary
    """
    # Verify signature
    try:
        sig = reader.read_string(4)
    except UnicodeDecodeError:
        print('\n' + reader.offset_to_string() + \
              '\terror: Failed to decode signature, wrong position?')
        sys.exit(1)
    if sig != '$AM1':
        reader.error('ADSP Manifest NOT found!', -4)
        sys.exit(1)
    reader.info('ADSP Manifest (' + sig + ')', -4)

    hdr = Component('adsp_mft_hdr', 'ADSP Manifest Header',
                    reader.get_offset() -4)
    hdr.add_a(Astring('sig', sig))

    hdr.add_a(Auint('size', reader.read_dw()))
    hdr.add_a(Astring('name', chararr_to_string(reader.read_bytes(8), 8)))
    hdr.add_a(Auint('preload', reader.read_dw()))
    hdr.add_a(Auint('fw_image_flags', reader.read_dw()))
    hdr.add_a(Auint('feature_mask', reader.read_dw()))
    hdr.add_a(Aversion('build_version', reader.read_w(), reader.read_w(),
                       reader.read_w(), reader.read_w()))

    hdr.add_a(Adec('num_module_entries', reader.read_dw()))
    hdr.add_a(Ahex('hw_buf_base_addr', reader.read_dw()))
    hdr.add_a(Auint('hw_buf_length', reader.read_dw()))
    hdr.add_a(Ahex('load_offset', reader.read_dw()))

    return hdr

def parse_adsp_manifest_mod_entry(index, reader):
    """ Parses ADSP manifest module entry from sof binary
    """
    # Verify Mod Entry signature
    try:
        sig = reader.read_string(4)
    except UnicodeDecodeError:
        print(reader.offset_to_string() + \
              '\terror: Failed to decode ModuleEntry signature')
        sys.exit(1)
    if sig != '$AME':
        reader.error('ModuleEntry signature NOT found!')
        sys.exit(1)
    reader.info('Module Entry signature found (' + sig + ')', -4)

    mod = AdspModuleEntry('mod_entry_'+repr(index),
                          reader.get_offset() -4)
    mod.add_a(Astring('sig', sig))

    mod.add_a(Astring('mod_name',
                      chararr_to_string(reader.read_bytes(8), 8)))
    mod.add_a(Astring('uuid', reader.read_uuid()))
    me_type = reader.read_dw()
    mod.add_a(Astring('type',
                      hex(me_type) + ' ' + mod_type_to_string(me_type)))
    mod.add_a(Abytes('hash', reader.read_bytes(32)))
    mod.add_a(Ahex('entry_point', reader.read_dw()))
    mod.add_a(Adec('cfg_offset', reader.read_w()))
    mod.add_a(Adec('cfg_count', reader.read_w()))
    mod.add_a(Auint('affinity_mask', reader.read_dw()))
    mod.add_a(Adec('instance_max_count', reader.read_w()))
    mod.add_a(Auint('instance_stack_size', reader.read_w()))
    for i in range(0, 3):
        seg_flags = reader.read_dw()
        mod.add_a(Astring('seg_'+repr(i)+'_flags',
                          hex(seg_flags) + ' ' + seg_flags_to_string(seg_flags)))
        mod.add_a(Ahex('seg_'+repr(i)+'_v_base_addr', reader.read_dw()))
        mod.add_a(Ahex('seg_'+repr(i)+'_size', ((seg_flags>>16)&0xffff)*0x1000))
        mod.add_a(Ahex('seg_'+repr(i)+'_file_offset', reader.read_dw()))

    return mod

def parse_adsp_manifest(reader, name):
    """ Parses ADSP manifest from sof binary
    """
    adsp_mft = AdspManifest(name, reader.get_offset())
    adsp_mft.add_comp(parse_adsp_manifest_hdr(reader))
    num_module_entries = adsp_mft.cdir['adsp_mft_hdr'].adir['num_module_entries'].val
    for i in range(0, num_module_entries):
        mod_entry = parse_adsp_manifest_mod_entry(i, reader)
        adsp_mft.add_comp(mod_entry)

    return adsp_mft

def parse_fw_bin(path, no_cse, verbose):
    """ Parses sof binary
    """
    reader = BinReader(path, verbose)

    parsed_bin = FwBin()
    parsed_bin.add_a(Astring('file_name', reader.file_name))
    parsed_bin.add_a(Auint('file_size', reader.file_size))
    parsed_bin.add_comp(parse_extended_manifest(reader))
    if not no_cse:
        parsed_bin.add_comp(parse_cse_manifest(reader))
    reader.set_offset(reader.ext_mft_length + 0x2000)
    parsed_bin.add_comp(parse_adsp_manifest(reader, 'cavs0015'))

    reader.info('Parsing finished', show_offset = False)
    return parsed_bin

class BinReader():
    """ sof binary reader
    """
    def __init__(self, path, verbose):
        self.verbose = verbose
        self.cur_offset = 0
        self.ext_mft_length = 0
        self.info('Reading SOF ri image ' + path, show_offset=False)
        self.file_name = path
        # read the content
        self.data = open(path, 'rb').read()
        self.file_size = len(self.data)
        self.info('File size ' + uint_to_string(self.file_size, True),
                  show_offset=False)

    def get_offset(self):
        """ Retrieve the offset, the reader is at
        """
        return self.cur_offset

    def ff_data(self, delta_offset):
        """ Forwards the read pointer by specified number of bytes
        """
        self.cur_offset += delta_offset

    def set_offset(self, offset):
        """ Set current reader offset
        """
        old_offset = self.cur_offset
        self.cur_offset = offset
        return old_offset

    def get_data(self, beg, length):
        """ Retrieves the data from beg to beg+length.
            This one is good to peek the data w/o advancing the read pointer
        """
        return self.data[self.cur_offset +beg : self.cur_offset +beg +length]

    def read_bytes(self, count):
        """ Reads the specified number of bytes from the stream
        """
        bts = self.get_data(0, count)
        self.ff_data(count)
        return bts

    def read_dw(self):
        """ Reads a dword from the stream
        """
        dword, = struct.unpack('I', self.get_data(0, 4))
        self.ff_data(4)
        return dword

    def read_w(self):
        """ Reads a word from the stream
        """
        word, = struct.unpack('H', self.get_data(0, 2))
        self.ff_data(2)
        return word

    def read_b(self):
        """ Reads a byte from the stream
        """
        byte, = struct.unpack('B', self.get_data(0, 1))
        self.ff_data(1)
        return byte

    def read_string(self, size_in_file):
        """ Reads a string from the stream, potentially padded with zeroes
        """
        return self.read_bytes(size_in_file).decode().rstrip('\0')

    def read_uuid(self):
        """ Reads a UUID from the stream and returns as string
        """
        out = '{:08x}'.format(self.read_dw())
        out += '-'+'{:04x}'.format(self.read_w())
        out += '-'+'{:04x}'.format(self.read_w())
        out += '-'+'{:02x}'.format(self.read_b()) + \
               '{:02x}'.format(self.read_b()) + '-'
        for _ in range(0, 6):
            out += '{:02x}'.format(self.read_b())
        return out

    def offset_to_string(self, delta=0):
        """ Retrieves readable representation of the current offset value
        """
        return uint_to_string(self.cur_offset+delta)

    def info(self, loginfo, off_delta=0, verb_info=True, show_offset=True):
        """ Prints 'info' log to the output, respects verbose mode
        """
        if verb_info and not self.verbose:
            return
        if show_offset:
            print(self.offset_to_string(off_delta) + '\t' + loginfo)
        else:
            print(loginfo)

    def error(self, logerror, off_delta=0):
        """ Prints 'error' log to the output
        """
        print(self.offset_to_string(off_delta) + '\terror: ' + logerror)

# Data Model

class Attribute():
    """ Attribute: base class with global formatting options
    """
    no_colors = False
    full_bytes = True

class Auint(Attribute):
    """ Attribute : unsigned integer
    """
    def __init__(self, name, val, color='none'):
        self.name = name
        self.val = val
        self.color = color

    def __str__(self):
        if Attribute.no_colors:
            return uint_to_string(self.val)
        return '{}{}{}'.format(change_color(self.color),
                               uint_to_string(self.val),
                               change_color('none'))

class Ahex(Attribute):
    """ Attribute : unsigned integer printed as hex
    """
    def __init__(self, name, val, color='none'):
        self.name = name
        self.val = val
        self.color = color

    def __str__(self):
        if Attribute.no_colors:
            return hex(self.val)
        return '{}{}{}'.format(change_color(self.color), hex(self.val),
                               change_color('none'))

class Adec(Attribute):
    """ Attribute: integer printed as dec
    """
    def __init__(self, name, val):
        self.name = name
        self.val = val

    def __str__(self):
        return repr(self.val)

class Abytes(Attribute):
    """ Attribute: array of bytes
    """
    def __init__(self, name, val, color='none'):
        self.name = name
        self.val = val
        self.color = color

    def __str__(self):
        length = len(self.val)
        if Attribute.no_colors:
            out = ''
        else:
            out = '{}'.format(change_color(self.color))
        if length <= 16:
            out += ' '.join(['{:02x}'.format(b) for b in self.val])
        elif Attribute.full_bytes:
            """ n is num pre row
                print 8 num pre line, useful for add more KEY
            """
            n = 8
            out += '\n'
            out += ',\n'.join([', '.join(['0x{:02x}'.format(b) for b in self.val[i:i + n]]) for i in range(0, length, n)])
        else:
            out += ' '.join('{:02x}'.format(b) for b in self.val[:8])
            out += ' ... '
            out += ' '.join('{:02x}'.format(b) for b in self.val[length-8:length])
        if not Attribute.no_colors:
            out += '{}'.format(change_color('none'))
        return out

class Adate(Attribute):
    """ Attribute: Date in BCD format
    """
    def __init__(self, name, val):
        self.name = name
        self.val = val

    def __str__(self):
        return date_to_string(self.val)

class Astring(Attribute):
    """ Attribute: String
    """
    def __init__(self, name, val):
        self.name = name
        self.val = val

    def __str__(self):
        return self.val

class Aversion(Attribute):
    """ Attribute: version
    """
    def __init__(self, name, major, minor, hotfix, build):
        self.name = name
        self.val = '{:d}.{:d}.{:d}.{:d}'.format(major, minor, hotfix, build)

    def __str__(self):
        return self.val

class Amodulus(Abytes):
    """ Attribute: modulus from RSA public key
    """
    def __init__(self, name, val, val_type):
        super().__init__(name, val)
        self.val_type = val_type

    def __str__(self):
        out = super().__str__()
        if not Attribute.full_bytes:
            if Attribute.no_colors:
                out += ' ({})'.format(self.val_type)
            else:
                out += ' {}({}){}'.format(change_color('red'), self.val_type,
                                          change_color('none'))
        return out

class Component():
    """ A component of sof binary
    """
    def __init__(self, uid, name, file_offset):
        self.uid = uid
        self.name = name
        self.file_offset = file_offset
        self.attribs = []
        self.adir = {}
        self.max_attr_name_len = 0
        self.components = []
        self.cdir = {}

    def add_a(self, attrib):
        """ Adds an attribute
        """
        self.max_attr_name_len = max(self.max_attr_name_len,
                                     len(attrib.name))
        self.attribs.append(attrib)
        self.adir[attrib.name] = attrib

    def add_comp(self, comp):
        """ Adds a nested component
        """
        self.components.append(comp)
        self.cdir[comp.uid] = comp

    def get_comp(self, comp_uid):
        """ Retrieves a nested component by id
        """
        for comp in self.components:
            if comp.uid == comp_uid:
                return comp
        return None

    def dump_info(self, pref, comp_filter):
        """ Prints out the content (name, all attributes, and nested comps)
        """
        print(pref + self.name)
        for attrib in self.attribs:
            print("{:}  {:<{:}} {:}".format(pref, attrib.name,
                                            self.max_attr_name_len, attrib))
        self.dump_comp_info(pref, comp_filter)

    def dump_attrib_info(self, pref, attr_name):
        """ Prints out a single attribute
        """
        attrib = self.adir[attr_name]
        print("{:}  {:<{:}} {:}".format(pref, attrib.name,
                                        self.max_attr_name_len, attrib))

    def dump_comp_info(self, pref, comp_filter=''):
        """ Prints out all nested components (filtered by name set to 'filter')
        """
        for comp in self.components:
            if comp.name in comp_filter:
                continue
            print()
            comp.dump_info(pref + '  ', comp_filter)

    def add_comp_to_mem_map(self, mem_map):
        for comp in self.components:
            comp.add_comp_to_mem_map(mem_map)

class ExtendedManifestAE1(Component):
    """ Extended manifest
    """
    def __init__(self):
        super(ExtendedManifestAE1, self).__init__('ext_mft',
                                               'Extended Manifest', 0)

    def dump_info(self, pref, comp_filter):
        hdr = self.cdir['ext_mft_hdr']
        if hdr.adir['length'].val == 0:
            return
        out = '{}{}'.format(pref, self.name)
        out += ' ver {}'.format(hdr.adir['ver'])
        out += ' entries {}'.format(hdr.adir['entries'])
        print(out)
        self.dump_comp_info(pref, comp_filter + ['Header'])

class ExtendedManifestXMan(Component):
    """ Extended manifest
    """
    def __init__(self):
        super(ExtendedManifestXMan, self).__init__('ext_mft',
                                               'Extended Manifest', 0)

    def dump_info(self, pref, comp_filter):
        hdr = self.cdir['ext_mft_hdr']
        if hdr.adir['length'].val == 0:
            return
        out = '{}{}'.format(pref, self.name)
        out += ' ver {}'.format(hdr.adir['ver'])
        out += ' length {}'.format(hdr.adir['length'].val)
        print(out)
        self.dump_comp_info(pref, comp_filter + ['Header'])

class CseManifest(Component):
    """ CSE Manifest
    """
    def __init__(self, offset):
        super(CseManifest, self).__init__('cse_mft', 'CSE Manifest', offset)

    def dump_info(self, pref, comp_filter):
        hdr = self.cdir['cse_mft_hdr']
        print('{}{} ver {} checksum {} partition name {}'.
              format(pref,
                     self.name, hdr.adir['header_version'],
                     hdr.adir['checksum'], hdr.adir['partition_name']))
        self.dump_comp_info(pref, comp_filter + ['Header'])

class CssManifest(Component):
    """ CSS Manifest
    """
    def __init__(self, name, offset):
        super(CssManifest, self).__init__('css_mft', name, offset)

    def dump_info(self, pref, comp_filter):
        hdr = self.cdir['css_mft_hdr']
        out = '{}{} (CSS Manifest)'.format(pref, self.name)
        out += ' type {}'.format(hdr.adir['type'])
        out += ' ver {}'.format(hdr.adir['header_version'])
        out += ' date {}'.format(hdr.adir['date'])
        print(out)
        print('{}  Rsvd0 {}'.
              format(pref, hdr.adir['reserved0']))
        print('{}  Modulus size (dwords) {}'.
              format(pref, hdr.adir['modulus_size']))
        print('{}    {}'.format(pref, hdr.adir['modulus']))
        print('{}  Exponent size (dwords) {}'.
              format(pref,
                     hdr.adir['exponent_size']))
        print('{}    {}'.format(pref, hdr.adir['exponent']))
        print('{}  Signature'.format(pref))
        print('{}    {}'.format(pref, hdr.adir['signature']))
        # super().dump_info(pref)
        self.dump_comp_info(pref, comp_filter + ['Header'])

class MftExtension(Component):
    """ Manifest Extension
    """
    def __init__(self, ext_id, name, offset):
        super(MftExtension, self).__init__('mft_ext'+repr(ext_id), name,
                                           offset)

    def dump_info(self, pref, comp_filter):
        print('{}{} type {} length {}'.
              format(pref, self.name,
                     self.adir['type'], self.adir['length']))

class PlatFwAuthExtension(MftExtension):
    """ Platform FW Auth Extension
    """
    def __init__(self, ext_id, offset):
        super(PlatFwAuthExtension,
              self).__init__(ext_id, 'Plat Fw Auth Extension', offset)

    def dump_info(self, pref, comp_filter):
        out = '{}{}'.format(pref, self.name)
        out += ' name {}'.format(self.adir['name'])
        out += ' vcn {}'.format(self.adir['vcn'])
        out += ' bitmap {}'.format(self.adir['bitmap'])
        out += ' svn {}'.format(self.adir['svn'])
        print(out)

class AdspMetadataFileExt(MftExtension):
    """ ADSP Metadata File Extension
    """
    def __init__(self, ext_id, offset):
        super(AdspMetadataFileExt,
              self).__init__(ext_id, 'ADSP Metadata File Extension',
                             offset)

    def dump_info(self, pref, comp_filter):
        out = '{}{}'.format(pref, self.name)
        out += ' ver {}'.format(self.adir['version'])
        out += ' base offset {}'.format(self.adir['base_offset'])
        out += ' limit offset {}'.format(self.adir['limit_offset'])
        print(out)
        print('{}  IMR type {}'.format(pref, self.adir['adsp_imr_type']))
        print('{}  Attributes'.format(pref))
        print('{}    {}'.format(pref, self.adir['attributes']))

class AdspManifest(Component):
    """ ADSP Manifest
    """
    def __init__(self, name, offset):
        super(AdspManifest, self).__init__('adsp_mft', name, offset)

    def dump_info(self, pref, comp_filter):
        hdr = self.cdir['adsp_mft_hdr']
        out = '{}{} (ADSP Manifest)'.format(pref, self.name)
        out += ' name {}'.format(hdr.adir['name'])
        out += ' build ver {}'.format(hdr.adir['build_version'])
        out += ' feature mask {}'.format(hdr.adir['feature_mask'])
        out += ' image flags {}'.format(hdr.adir['fw_image_flags'])
        print(out)
        print('{}  HW buffers base address {} length {}'.
              format(pref,
                     hdr.adir['hw_buf_base_addr'],
                     hdr.adir['hw_buf_length']))
        print('{}  Load offset {}'.format(pref,
                                          hdr.adir['load_offset']))
        self.dump_comp_info(pref, comp_filter + ['ADSP Manifest Header'])

class AdspModuleEntry(Component):
    """ ADSP Module Entry
    """
    def __init__(self, uid, offset):
        super(AdspModuleEntry, self).__init__(uid, 'Module Entry', offset)

    def dump_info(self, pref, comp_filter):
        print('{}{:9} {}'.format(pref, str(self.adir['mod_name']),
            self.adir['uuid']))
        print('{}  entry point {} type {}'.format(pref, self.adir['entry_point'],
            self.adir['type']))
        out = '{}  cfg offset {} count {} affinity {}'.format(pref,
            self.adir['cfg_offset'], self.adir['cfg_count'],
            self.adir['affinity_mask'])
        out += ' instance max count {} stack size {}'.format(
            self.adir['instance_max_count'], self.adir['instance_stack_size'])
        print(out)
        print('{}  .text   {} file offset {} flags {}'.format(pref,
            self.adir['seg_0_v_base_addr'], self.adir['seg_0_file_offset'],
            self.adir['seg_0_flags']))
        print('{}  .rodata {} file offset {} flags {}'.format(pref,
            self.adir['seg_1_v_base_addr'], self.adir['seg_1_file_offset'],
            self.adir['seg_1_flags']))
        print('{}  .bss    {} file offset {} flags {}'.format(pref,
            self.adir['seg_2_v_base_addr'], self.adir['seg_2_file_offset'],
            self.adir['seg_2_flags']))

    def add_comp_to_mem_map(self, mem_map):
        mem_map.insert_segment(DspMemorySegment(
            self.adir['mod_name'].val + '.text',
            self.adir['seg_0_v_base_addr'].val,
            self.adir['seg_0_size'].val));
        mem_map.insert_segment(DspMemorySegment(
            self.adir['mod_name'].val + '.rodata',
            self.adir['seg_1_v_base_addr'].val,
            self.adir['seg_1_size'].val));
        mem_map.insert_segment(DspMemorySegment(
            self.adir['mod_name'].val + '.bss',
            self.adir['seg_2_v_base_addr'].val,
            self.adir['seg_2_size'].val));

class FwBin(Component):
    """ Parsed sof binary
    """
    def __init__(self):
        super(FwBin, self).__init__('bin', 'SOF Binary', 0)

    def dump_info(self, pref, comp_filter):
        """ Print out the content
        """
        print('SOF Binary {} size {}'.format(
            self.adir['file_name'], self.adir['file_size']))
        self.dump_comp_info(pref, comp_filter)

    def populate_mem_map(self, mem_map):
        """ Adds modules' segments to the memory map
        """
        self.add_comp_to_mem_map(mem_map)

# DSP Memory Layout
def get_mem_map(ri_path):
    """ Retrieves memory map for platform determined by the file name
    """
    for plat_name in DSP_MEM_SPACE_EXT:
        # use full firmware name for match
        if "sof-{}.ri".format(plat_name) in ri_path:
            return DSP_MEM_SPACE_EXT[plat_name]
    return DspMemory('Memory layout undefined', [])

def add_lmap_mem_info(ri_path, mem_map):
    """ Optional lmap processing
    """
    lmap_path = ri_path[0:ri_path.rfind('.')] + '.lmap'
    try:
        with open(lmap_path) as lmap:
            it_lines = iter(lmap.readlines())
            for line in it_lines:
                if 'Sections:' in line:
                    next(it_lines)
                    break;
            for line in it_lines:
                tok = line.split()
                mem_map.insert_segment(DspMemorySegment(tok[1],
                    int(tok[3], 16), int(tok[2], 16)))
                next(it_lines)

    except FileNotFoundError:
        return

class DspMemorySegment(object):
    """ Single continuous memory space
    """
    def __init__(self, name, base_address, size):
        self.name = name
        self.base_address = base_address
        self.size = size
        self.used_size = 0
        self.inner_segments = []

    def is_inner(self, segment):
        return self.base_address <= segment.base_address and \
            segment.base_address + segment.size <= self.base_address + self.size

    def insert_segment(self, segment):
        for seg in self.inner_segments:
            if seg.is_inner(segment):
                seg.insert_segment(segment)
                return
        self.inner_segments.append(segment)
        self.used_size += segment.size

    def dump_info(self, pref):
        free_size = self.size - self.used_size
        out = '{}{:<35} 0x{:x}'.format(pref, self.name, self.base_address)
        if self.used_size > 0:
            out += ' ({} + {}  {:.2f}% used)'.format(self.used_size, free_size,
                self.used_size*100/self.size)
        else:
            out += ' ({})'.format(free_size)
        print(out)
        for seg in self.inner_segments:
            seg.dump_info(pref + '  ')

class DspMemory(object):
    """ Dsp Memory, all top-level segments
    """
    def __init__(self, platform_name, segments):
        self.platform_name = platform_name
        self.segments = segments

    def insert_segment(self, segment):
        """ Inserts segment
        """
        for seg in self.segments:
            if seg.is_inner(segment):
                seg.insert_segment(segment)
                return

    def dump_info(self):
        print(self.platform_name)
        for seg in self.segments:
            seg.dump_info('  ')

# Layouts of DSP memory for known platforms

APL_MEMORY_SPACE = DspMemory('Intel Apollolake',
    [
        DspMemorySegment('imr', 0xa0000000, 4*1024*1024),
        DspMemorySegment('l2 hpsram', 0xbe000000, 8*64*1024),
        DspMemorySegment('l2 lpsram', 0xbe800000, 2*64*1024)
    ])

CNL_MEMORY_SPACE = DspMemory('Intel Cannonlake',
    [
        DspMemorySegment('imr', 0xb0000000, 8*0x1024*0x1024),
        DspMemorySegment('l2 hpsram', 0xbe000000, 48*64*1024),
        DspMemorySegment('l2 lpsram', 0xbe800000, 1*64*1024)
    ])

ICL_MEMORY_SPACE = DspMemory('Intel Icelake',
    [
        DspMemorySegment('imr', 0xb0000000, 8*1024*1024),
        DspMemorySegment('l2 hpsram', 0xbe000000, 47*64*1024),
        DspMemorySegment('l2 lpsram', 0xbe800000, 1*64*1024)
    ])

TGL_LP_MEMORY_SPACE = DspMemory('Intel Tigerlake-LP',
    [
        DspMemorySegment('imr', 0xb0000000,16*1024*1024),
        DspMemorySegment('l2 hpsram', 0xbe000000, 46*64*1024),
        DspMemorySegment('l2 lpsram', 0xbe800000, 1*64*1024)
    ])

JSL_MEMORY_SPACE = DspMemory('Intel Jasperlake',
    [
        DspMemorySegment('imr', 0xb0000000, 8*1024*1024),
        DspMemorySegment('l2 hpsram', 0xbe000000, 16*64*1024),
        DspMemorySegment('l2 lpsram', 0xbe800000, 1*64*1024)
    ])

TGL_H_MEMORY_SPACE = DspMemory('Intel Tigerlake-H',
    [
        DspMemorySegment('imr', 0xb0000000, 16*1024*1024),
        DspMemorySegment('l2 hpsram', 0xbe000000, 30*64*1024),
        DspMemorySegment('l2 lpsram', 0xbe800000, 1*64*1024)
    ])

DSP_MEM_SPACE_EXT = {
    'apl' : APL_MEMORY_SPACE,
    'cnl' : CNL_MEMORY_SPACE,
    'icl' : ICL_MEMORY_SPACE,
    'tgl' : TGL_LP_MEMORY_SPACE,
    'jsl' : JSL_MEMORY_SPACE,
    'tgl-h' : TGL_H_MEMORY_SPACE,
    'ehl' : TGL_LP_MEMORY_SPACE,
}

def main(args):
    """ main function
    """
    if sys.stdout.isatty():
        Attribute.no_colors = args.no_colors
    else:
        Attribute.no_colors = True

    Attribute.full_bytes = args.full_bytes

    fw_bin = parse_fw_bin(args.sof_ri_path, args.no_cse, args.verbose)

    comp_filter = []
    if args.headers or args.no_modules:
        comp_filter.append('Module Entry')
    if args.no_headers:
        comp_filter.append('CSE Manifest')
    fw_bin.dump_info('', comp_filter)
    if not args.no_memory:
        mem = get_mem_map(args.sof_ri_path)
        fw_bin.populate_mem_map(mem)
        add_lmap_mem_info(args.sof_ri_path, mem)
        print()
        mem.dump_info()

if __name__ == "__main__":
    ARGS = parse_params()
    main(ARGS)
