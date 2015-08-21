// window-vectors.S - Register Window Overflow/Underflow Handlers
// $Id: //depot/rel/Eaglenest/Xtensa/OS/xtos/window-vectors.S#1 $

// Copyright (c) 1999-2013 Tensilica Inc.
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

#if XCHAL_HAVE_WINDOWED && !defined(__XTENSA_CALL0_ABI__)

// Simply include the correct version of the actual handlers

#if XCHAL_HAVE_XEA1
#include "window-vectors-xea1.S"
#else
#include "window-vectors-new.S"
#endif

#endif /* XCHAL_HAVE_WINDOWED && !defined(__XTENSA_CALL0_ABI__) */

