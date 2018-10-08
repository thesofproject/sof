// 
// mpu_asm.S - Assembler routine(s) for the MPU
//
// $Id$

// Copyright (c) 2015 Tensilica Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <xtensa/coreasm.h>
#include <xtensa/mpuasm.h>

#if defined(__SPLIT__write_map_raw) ||\
    defined(__SPLIT__write_map_raw_nw)

/*
  void  xthal_write_map_raw( const struct xthal_MPU_entry* map, unsigned n);

  Updates the MPU with the MPU entries provided:
	map	pointer to array of MPU entries
	n	number of entries in array (0 <= n <= XCHAL_MPU_ENTRIES)

  The entries provided must have monotonically increasing addresses.
  This function otherwise orders its updates to ensure the MPU always has
  all its entries in monotonically increasing sequence.

  on entry
  	a2 		=>	vector of MPU entries to write
  	a3		=>	number of entries to write
  	a4-a7	=>	destroyed
*/

DECLFUNC (xthal_write_map_raw)
	abi_entry
	mpu_write_map a2, a3, a4, a5, a6, a7
	abi_return
	endfunc

#endif

/*
	void xthal_read_map_raw(struct xthal_MPU_entry* map);

	Reads the current map from the MPU and puts it the vector
	pointed to by map.

	a2		=> 	vector of MPU entries where the current MPU state is copied
	a3-a4	=>	destroyed
*/

#if defined(__SPLIT__read_map_raw) ||\
    defined(__SPLIT__read_map_raw_nw)

DECLFUNC (xthal_read_map_raw)
	abi_entry
	mpu_read_map a2, a3, a4
	abi_return
	endfunc

#endif

