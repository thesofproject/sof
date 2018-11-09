/*
* debug log converter interface.
*
* Copyright (c) 2018, Intel Corporation.
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
*
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*/
#include "convert.h"

double to_usecs(uint64_t time, double clk)
{
	/* trace timestamp uses CPU system clock at default 25MHz ticks */
	// TODO: support variable clock rates
	return (double)time / clk;
}
