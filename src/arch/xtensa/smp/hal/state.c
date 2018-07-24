// 
// processor_state.c - processor state management routines
//

// Copyright (c) 2005-2010 Tensilica Inc.
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

#include <xtensa/hal.h>
#include <xtensa/config/core.h>


//----------------------------------------------------------------------

#if defined(__SPLIT__extra_size)
// space for "extra" (user special registers and non-coprocessor TIE) state:
const unsigned int Xthal_extra_size = XCHAL_NCP_SA_SIZE;

#elif defined(__SPLIT__extra_align)
const unsigned int Xthal_extra_align = XCHAL_NCP_SA_ALIGN;

#elif defined(__SPLIT__cpregs_size)
// space for state of TIE coprocessors:
const unsigned int Xthal_cpregs_size[8] =
	{
	    XCHAL_CP0_SA_SIZE,
	    XCHAL_CP1_SA_SIZE,
	    XCHAL_CP2_SA_SIZE,
	    XCHAL_CP3_SA_SIZE,
	    XCHAL_CP4_SA_SIZE,
	    XCHAL_CP5_SA_SIZE,
	    XCHAL_CP6_SA_SIZE,
	    XCHAL_CP7_SA_SIZE
	};

#elif defined(__SPLIT__cpregs_align)
const unsigned int Xthal_cpregs_align[8] =
	{
	    XCHAL_CP0_SA_ALIGN,
	    XCHAL_CP1_SA_ALIGN,
	    XCHAL_CP2_SA_ALIGN,
	    XCHAL_CP3_SA_ALIGN,
	    XCHAL_CP4_SA_ALIGN,
	    XCHAL_CP5_SA_ALIGN,
	    XCHAL_CP6_SA_ALIGN,
	    XCHAL_CP7_SA_ALIGN
	};

#elif defined(__SPLIT__cp_names)
const char * const Xthal_cp_names[8] =
	{
	    XCHAL_CP0_NAME,
	    XCHAL_CP1_NAME,
	    XCHAL_CP2_NAME,
	    XCHAL_CP3_NAME,
	    XCHAL_CP4_NAME,
	    XCHAL_CP5_NAME,
	    XCHAL_CP6_NAME,
	    XCHAL_CP7_NAME
	};

#elif defined(__SPLIT__all_extra_size)
// total save area size (extra + all coprocessors + min 16-byte alignment everywhere)
const unsigned int Xthal_all_extra_size = XCHAL_TOTAL_SA_SIZE;

#elif defined(__SPLIT__all_extra_align)
// maximum required alignment for the total save area (this might be useful):
const unsigned int Xthal_all_extra_align = XCHAL_TOTAL_SA_ALIGN;

#elif defined(__SPLIT__num_coprocessors)
// number of coprocessors starting contiguously from zero
// (same as Xthal_cp_max, but included for Tornado2):
const unsigned int Xthal_num_coprocessors = XCHAL_CP_MAX;

#elif defined(__SPLIT__cp_num)
// actual number of coprocessors:
const unsigned char Xthal_cp_num    = XCHAL_CP_NUM;

#elif defined(__SPLIT__cp_max)
// index of highest numbered coprocessor, plus one:
const unsigned char Xthal_cp_max    = XCHAL_CP_MAX;

// index of highest allowed coprocessor number, per cfg, plus one:
//const unsigned char Xthal_cp_maxcfg = XCHAL_CP_MAXCFG;

#elif defined(__SPLIT__cp_mask)
// bitmask of which coprocessors are present:
const unsigned int  Xthal_cp_mask   = XCHAL_CP_MASK;

#elif defined(__SPLIT__cp_id_mappings)
// Coprocessor ID from its name

# ifdef XCHAL_CP0_IDENT
const unsigned char XCJOIN(Xthal_cp_id_,XCHAL_CP0_IDENT) = 0;
# endif
# ifdef XCHAL_CP1_IDENT
const unsigned char XCJOIN(Xthal_cp_id_,XCHAL_CP1_IDENT) = 1;
# endif
# ifdef XCHAL_CP2_IDENT
const unsigned char XCJOIN(Xthal_cp_id_,XCHAL_CP2_IDENT) = 2;
# endif
# ifdef XCHAL_CP3_IDENT
const unsigned char XCJOIN(Xthal_cp_id_,XCHAL_CP3_IDENT) = 3;
# endif
# ifdef XCHAL_CP4_IDENT
const unsigned char XCJOIN(Xthal_cp_id_,XCHAL_CP4_IDENT) = 4;
# endif
# ifdef XCHAL_CP5_IDENT
const unsigned char XCJOIN(Xthal_cp_id_,XCHAL_CP5_IDENT) = 5;
# endif
# ifdef XCHAL_CP6_IDENT
const unsigned char XCJOIN(Xthal_cp_id_,XCHAL_CP6_IDENT) = 6;
# endif
# ifdef XCHAL_CP7_IDENT
const unsigned char XCJOIN(Xthal_cp_id_,XCHAL_CP7_IDENT) = 7;
# endif

#elif defined(__SPLIT__cp_mask_mappings)
// Coprocessor "mask" (1 << ID) from its name

# ifdef XCHAL_CP0_IDENT
const unsigned int  XCJOIN(Xthal_cp_mask_,XCHAL_CP0_IDENT) = (1 << 0);
# endif
# ifdef XCHAL_CP1_IDENT
const unsigned int  XCJOIN(Xthal_cp_mask_,XCHAL_CP1_IDENT) = (1 << 1);
# endif
# ifdef XCHAL_CP2_IDENT
const unsigned int  XCJOIN(Xthal_cp_mask_,XCHAL_CP2_IDENT) = (1 << 2);
# endif
# ifdef XCHAL_CP3_IDENT
const unsigned int  XCJOIN(Xthal_cp_mask_,XCHAL_CP3_IDENT) = (1 << 3);
# endif
# ifdef XCHAL_CP4_IDENT
const unsigned int  XCJOIN(Xthal_cp_mask_,XCHAL_CP4_IDENT) = (1 << 4);
# endif
# ifdef XCHAL_CP5_IDENT
const unsigned int  XCJOIN(Xthal_cp_mask_,XCHAL_CP5_IDENT) = (1 << 5);
# endif
# ifdef XCHAL_CP6_IDENT
const unsigned int  XCJOIN(Xthal_cp_mask_,XCHAL_CP6_IDENT) = (1 << 6);
# endif
# ifdef XCHAL_CP7_IDENT
const unsigned int  XCJOIN(Xthal_cp_mask_,XCHAL_CP7_IDENT) = (1 << 7);
# endif

//----------------------------------------------------------------------

#elif defined(__SPLIT__init_mem_extra)
// CMS: I have made the assumptions that 0's are safe initial
// values. That may be wrong at some point.
//
// initialize the extra processor
void
xthal_init_mem_extra(void *address)
/* not clear that it is safe to call memcpy and also not clear
   that performance is important. */
{
    unsigned int *ptr;
    unsigned int *end;

    ptr = (unsigned int *)address;
    end = (unsigned int *)((int)address + XCHAL_NCP_SA_SIZE);
    while( ptr < end )
    {
	*ptr++ = 0;
    } 
}

#elif defined(__SPLIT__init_mem_cp)
// initialize the TIE coprocessor
void
xthal_init_mem_cp(void *address, int cp)
{
    unsigned int *ptr;
    unsigned int *end;

    if( cp <= 7 ) 
    {
	end = (unsigned int *)((int)address + Xthal_cpregs_size[cp]);
	ptr = (unsigned int *)address;
	while( ptr < end )
	{
	    *ptr++ = 0;
	} 
    }
}

#endif /*splitting*/


/*  Nothing implemented below this point.  */
/************************************************************************/

// save all extra+cp processor state (NOT IMPLEMENTED)
/*void xthal_save_all_extra(void *base)
{
    xthal_save_extra(base);
    ... here we need to iterate over configured coprocessor register files ...
//    xthal_save_cpregs(base+XCHAL_NCP_SA_SIZE, 0);
}*/

// restore all extra+cp processor state (NOT IMPLEMENTED)
/*void xthal_restore_all_extra(void *base)
{
    xthal_restore_extra(base);
    ... here we need to iterate over configured coprocessor register files ...
//    xthal_restore_cpregs(base+XCHAL_NCP_SA_SIZE, 0);
}*/


// initialize the extra processor (NOT IMPLEMENTED)
/*void xthal_init_extra()
{
}*/

// initialize the TIE coprocessor (NOT IMPLEMENTED)
/*void xthal_init_cp(int cp)
{
}*/


#if 0

/* read extra state register (NOT IMPLEMENTED) */
int xthal_read_extra(void *base, unsigned reg, unsigned *value)
{
	if (reg&0x1000) {
		switch(reg) {
#if XCHAL_HAVE_MAC16
			case 16:
				*value = ((unsigned *)base)[0];
				return reg;
			case 17:
				*value = ((unsigned *)base)[1];
				return reg;
			case 32:
				*value = ((unsigned *)base)[2];
				return reg;
			case 33:
				*value = ((unsigned *)base)[3];
				return reg;
			case 34:
				*value = ((unsigned *)base)[4];
				return reg;
			case 35:
				*value = ((unsigned *)base)[5];
				return reg;
#endif /* XCHAL_HAVE_MAC16 */
		}
	}
	return -1;
}

/* write extra state register (NOT IMPLEMENTED) */
int xthal_write_extra(void *base, unsigned reg, unsigned value)
{
	if (reg&0x1000) {
	    switch(reg) {
#if XCHAL_HAVE_MAC16
			case 16:
				((unsigned *)base)[0] = value;
				return reg;
			case 17:
				((unsigned *)base)[1] = value;
				return reg;
			case 32:
				((unsigned *)base)[2] = value;
				return reg;
			case 33:
				((unsigned *)base)[3] = value;
				return reg;
			case 34:
				((unsigned *)base)[4] = value;
				return reg;
			case 35:
				((unsigned *)base)[5] = value;
				return reg;
#endif /* XCHAL_HAVE_MAC16 */
		}
	}
	return -1;
}

#endif /*0*/


/* read TIE coprocessor register (NOT IMPLEMENTED) */
/*int xthal_read_cpreg(void *base, int cp, unsigned reg, unsigned *value)
{
    return -1;
}*/

/* write TIE coproessor register (NOT IMPLEMENTED) */
/*int xthal_write_cpreg(void *base, int cp, unsigned reg, unsigned value)
{
	return -1;
}*/

/* return coprocessor number based on register (NOT IMPLEMENTED) */
/*int xthal_which_cp(unsigned reg)
{
	return -1;
}*/

