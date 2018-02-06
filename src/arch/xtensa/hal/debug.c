// 
// debug.c - debug related constants and functions
//
// $Id: //depot/rel/Eaglenest/Xtensa/OS/hal/debug.c#1 $

// Copyright (c) 2002 Tensilica Inc.
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
#include <xtensa/config/specreg.h>
#include <xtensa/config/core.h>


/*  1 if debug option configured, 0 if not:  */
const int Xthal_debug_configured = XCHAL_HAVE_DEBUG;

/*  Number of instruction and data break registers:  */
const int Xthal_num_ibreak = XCHAL_NUM_IBREAK;
const int Xthal_num_dbreak = XCHAL_NUM_DBREAK;


#ifdef INCLUDE_DEPRECATED_HAL_DEBUG_CODE
/*  This array is incorrect:  */
const unsigned short	Xthal_ill_inst_16[16] =
{
#if XCHAL_HAVE_BE
    0xfd0f, 0xfd1f, 0xfd2f, 0xfd3f,
    0xfd4f, 0xfd5f, 0xfd6f, 0xfd7f,
    0xfd8f, 0xfd9f, 0xfdaf, 0xfdbf,
    0xfdcf, 0xfddf, 0xfdef, 0xfdff
#else
    0xf0fd, 0xf1fd, 0xf2fd, 0xf3fd,
    0xf4fd, 0xf5fd, 0xf6fd, 0xf7fd,
    0xf8fd, 0xf9fd, 0xfafd, 0xfbfd,
    0xfcfd, 0xfdfd, 0xfefd, 0xfffd
#endif
};
#endif /* INCLUDE_DEPRECATED_HAL_DEBUG_CODE */


#undef XTHAL_24_BIT_BREAK
#undef XTHAL_16_BIT_BREAK
#define XTHAL_24_BIT_BREAK		0x80000000
#define XTHAL_16_BIT_BREAK		0x40000000



// set software breakpoint and synchronize cache
unsigned int
xthal_set_soft_break(void *addr)
{
    unsigned inst;
    int is24bit = (xthal_disassemble_size( (unsigned char *)addr ) == 3);
    unsigned int ret_val;

#if XCHAL_HAVE_BE
    inst =  ((((char *)addr)[0])<<24) +
            ((((char *)addr)[1])<<16) +
            ((((char *)addr)[2])<<8);
#else
    inst =  ((((char *)addr)[0])) +
            ((((char *)addr)[1])<<8) +
            ((((char *)addr)[2])<<16);
#endif
#if XCHAL_HAVE_BE
    if (is24bit) {
	ret_val = XTHAL_24_BIT_BREAK & ((inst>>8)&0xffffff);
	((unsigned char *)addr)[0] = 0x00;
	((unsigned char *)addr)[1] = 0x04;
	((unsigned char *)addr)[2] = 0x00;
    } else {
	ret_val = XTHAL_16_BIT_BREAK & ((inst>>16)&0xffff);
	((unsigned char *)addr)[0] = 0xD2;
	((unsigned char *)addr)[1] = 0x0f;
    }
#else
    if (is24bit) {
	ret_val = XTHAL_24_BIT_BREAK & (inst&0xffffff);
	((unsigned char *)addr)[0] = 0x00;
	((unsigned char *)addr)[1] = 0x40;
	((unsigned char *)addr)[2] = 0x00;
    } else {
	ret_val = XTHAL_16_BIT_BREAK & (inst&0xffff);
	((unsigned char *)addr)[0] = 0x2D;
	((unsigned char *)addr)[1] = 0xf0;
    }
#endif
    *((unsigned int *)addr) = inst;
#if XCHAL_DCACHE_IS_WRITEBACK
    xthal_dcache_region_writeback((void*)addr, 3);
#endif
#if XCHAL_ICACHE_SIZE > 0
    xthal_icache_region_invalidate((void*)addr, 3);
#endif
    return ret_val;
}


// remove software breakpoint and synchronize cache
void
xthal_remove_soft_break(void *addr, unsigned int inst)
{
#if XCHAL_HAVE_BE
    if (inst&XTHAL_24_BIT_BREAK) {
	((unsigned char *)addr)[0] = (inst>>16)&0xff;
	((unsigned char *)addr)[1] = (inst>>8)&0xff;
	((unsigned char *)addr)[2] = inst&0xff;
    } else {
	((unsigned char *)addr)[0] = (inst>>8)&0xff;
	((unsigned char *)addr)[1] = inst&0xff;
    }
#else
    ((unsigned char *)addr)[0] = inst&0xff;
    ((unsigned char *)addr)[1] = (inst>>8)&0xff;
    if (inst&XTHAL_24_BIT_BREAK)
	((unsigned char *)addr)[2] = (inst>>16)&0xff;
#endif
#if XCHAL_DCACHE_IS_WRITEBACK
    xthal_dcache_region_writeback((void*)addr, 3);
#endif
#if XCHAL_ICACHE_SIZE > 0
    xthal_icache_region_invalidate((void*)addr, 3);
#endif
}




#ifdef INCLUDE_DEPRECATED_HAL_DEBUG_CODE

// return instruction type
unsigned int
xthal_inst_type(void *addr)
{
    unsigned int inst_type = 0;
    unsigned inst;
//    unsigned int inst = *((unsigned int *)addr);
    unsigned char op0, op1, op2;
    unsigned char i, m, n, r, s, t, z;

#if XCHAL_HAVE_BE
    inst =  ((((char *)addr)[0])<<24) +
            ((((char *)addr)[1])<<16) +
            ((((char *)addr)[2])<<8);
    op0 = inst>>28;
    op1 = (inst>>12)&0xf;
    op2 = (inst>>16)&0xf;
    i = (inst>>27)&0x1;
    z = (inst>>26)&0x1;
    m = (inst>>24)&0x3;
    n = (inst>>26)&0x3;
    r = (inst>>16)&0xf;
    s = (inst>>20)&0xf;
    t = (inst>>24)&0xf;
#else
    inst =  ((((char *)addr)[0])) +
            ((((char *)addr)[1])<<8) +
            ((((char *)addr)[2])<<16);
    op0 = inst&0xf;
    op1 = (inst&0xf0000)>>16;
    op2 = (inst&0xf00000)>>20;
    i = (inst&0x80)>>7;
    z = (inst&0x40)>>6;
    m = (inst&0xc0)>>6;
    n = (inst&0x30)>>4;
    r = (inst&0xf000)>>12;
    s = (inst&0xf00)>>8;
    t = (inst&0xf0)>4;
#endif
    switch (op0) {
	  case 0x0:
		inst_type |= XTHAL_24_BIT_INST;
		if ((op1==0)&&(op2==0))
			switch (r) {
			  case 0:
				if (m==0x2) {
				  if (!(n&0x2))		// RET, RETW
				    inst_type |= XTHAL_RET_INST;
				  else if (n==0x2)	// JX
				    inst_type |= (XTHAL_JUMP_INST|XTHAL_DEST_REG_INST);
					inst_type |= (s<<28);
				} else if (m==3)	// CALLX
				  inst_type |= (XTHAL_JUMP_INST|XTHAL_DEST_REG_INST);
				  inst_type |= (s<<28);
			  	break;
			  case 0x3:
			    if (t==0)
				  switch (s) {
				    case 0x0:	// RFE
					  inst_type |= XTHAL_RFE_INST;
					  break;
					case 0x1:   // RFUE
					  inst_type |= XTHAL_RFUE_INST;
					  break;
					case 0x4:	// RFW
					case 0x5:
					  inst_type |= XTHAL_RFW_INST;
					  break;
				  }
				else if (t==1)	// RFI
				  inst_type |= XTHAL_RFI_INST;
				break;
			  case 0x4:	// BREAK
		    	inst_type |= XTHAL_BREAK_INST;
				break;
			  case 0x5:	// SYSCALL
		    	inst_type |= XTHAL_SYSCALL_INST;
				break;
			}
		break;
	  case 0x5:	// CALL
	    inst_type |= XTHAL_24_BIT_INST;
	    inst_type |= (XTHAL_JUMP_INST|XTHAL_DEST_REL_INST);
	    break;
	  case 0x6:	// B
	    inst_type |= XTHAL_24_BIT_INST;
		if (n==0)	// J
		  inst_type |= (XTHAL_JUMP_INST|XTHAL_DEST_REL_INST);
		else if ((n==0x1)||(n==0x2))
		  inst_type |= (XTHAL_BRANCH_INST|XTHAL_DEST_REL_INST);
		else {
		  if (m&0x2)
		    inst_type |= (XTHAL_BRANCH_INST|XTHAL_DEST_REL_INST);
		  else if ((m==0x1)&&((r==0x0)||(r==0x1)))
		    inst_type |= (XTHAL_BRANCH_INST|XTHAL_DEST_REL_INST);
		}
	    break;
	  case 0x7:	// B
	    inst_type |= XTHAL_24_BIT_INST;
	    inst_type |= (XTHAL_BRANCH_INST|XTHAL_DEST_REL_INST);
	    break;
#if XCHAL_HAVE_DENSITY
	  case 0x8:	// L32I.N
	  case 0x9:	// S32I.N
	  case 0xA:	// ADD.N
	  case 0xb:	// ADDI.N
	    inst_type |= XTHAL_16_BIT_INST;
		break;
	  case 0xc:
		inst_type |= XTHAL_16_BIT_INST;	// MOVI.N BEQZ.N, BNEZ.N
		if (i)
			inst_type |= (XTHAL_BRANCH_INST|XTHAL_DEST_REL_INST);
		break;
	  case 0xd:			// MOV.N NOP.N
		inst_type |= XTHAL_16_BIT_INST;
		if (r==0xf)
			switch(t) {
			  case 0x0:
			  case 0x1:
				inst_type |= XTHAL_RET_INST;	// RET.N, RETW.N
				break;
			  case 0x2:
				inst_type |= XTHAL_BREAK_INST;	// BREAK.N
				break;
			}
		break;
#endif /* XCHAL_HAVE_DENSITY */
	  default:
		inst_type |= XTHAL_24_BIT_INST;
	}
	return inst_type;
}

// returns branch address
unsigned int
xthal_branch_addr(void *addr)
{
    unsigned int b_addr = (unsigned int) addr;
    unsigned inst;
//  unsigned int inst = *((unsigned int *)addr);
    int offset;
    unsigned int inst_type = xthal_inst_type(addr);
    unsigned int inst_type_mask;
#if XCHAL_HAVE_BE
    inst =  ((((char *)addr)[0])<<24) +
            ((((char *)addr)[1])<<16) +
            ((((char *)addr)[2])<<8);
#else
    inst =  ((((char *)addr)[0])) +
            ((((char *)addr)[1])<<8) +
            ((((char *)addr)[2])<<16);
#endif
#if XCHAL_HAVE_DENSITY
    inst_type_mask = XTHAL_16_BIT_INST|XTHAL_BRANCH_INST|XTHAL_DEST_REL_INST;
    if ((inst_type&inst_type_mask)==inst_type_mask) {
# if XCHAL_HAVE_BE
	b_addr += (4+((inst&0x3000000)>>20)+((inst&0xf0000)>>16));
# else
	b_addr += (4+(inst&0x30)+((inst&0xf000)>>12));
# endif
    }
#endif /* XCHAL_HAVE_DENSITY */
	inst_type_mask = XTHAL_24_BIT_INST|XTHAL_BRANCH_INST|XTHAL_DEST_REL_INST;
	if ((inst_type&inst_type_mask)==inst_type_mask) {
#if XCHAL_HAVE_BE
		if ((inst&0xf0000000)==0x70000000)
			offset = ((int)(inst<<16))>>24;
		else if ((inst&0xf2000000)==0x62000000)
			offset = ((int)(inst<<16))>>24;
		else
			offset = ((int)(inst<<12))>>20;
#else
		if ((inst&0xf)==0x7)
			offset = ((int)(inst<<8))>>24;
		else if ((inst&0x2f)==0x26)
			offset = ((int)(inst<<8))>>24;
		else
			offset = ((int)(inst<<8))>>20;
#endif
		b_addr += 4 + offset;
	}
	inst_type_mask = XTHAL_24_BIT_INST|XTHAL_JUMP_INST|XTHAL_DEST_REL_INST;
	if ((inst_type&inst_type_mask)==inst_type_mask) {
#if XCHAL_HAVE_BE
		if ((inst&0xfc000000)==0x60000000)
			offset = ((int)(inst<<6))>>14;
		else
		{
			b_addr &= 0xfffffffc;
			offset = ((int)(inst<<6))>>12;
		}
#else
		if ((inst&0x3f)==0x6)
			offset = ((int)(inst<<8))>>14;
		else
		{
			b_addr &= 0xfffffffc;
			offset = ((int)(inst<<8))>>12;
		}
#endif
		b_addr += 4 + offset;
	}
    return b_addr;
}

// return pc of next instruction for a given state
unsigned int xthal_get_npc(XTHAL_STATE *user_state)
{
    unsigned inst_type;
    unsigned npc;
    inst_type = xthal_inst_type((void *)user_state->pc);
    if (inst_type & XTHAL_24_BIT_INST)
        npc = user_state->pc + 3;
    else
        npc = user_state->pc + 2;
    if (inst_type & XTHAL_RFW_INST) {
	/* Can not debug level 1 interrupts */
	// xt_panic();
    } else if (inst_type & XTHAL_RFUE_INST) {
	/* Can not debug level 1 interrupts */
	// xt_panic();
    } else if (inst_type & XTHAL_RFI_INST) {
	/* Can not debug level 1 interrupts */
	// xt_panic();
    } else if (inst_type & XTHAL_RFE_INST) {
	/* Can not debug level 1 interrupts */
	// xt_panic();
    } else if (inst_type & XTHAL_RET_INST) {
	npc = (user_state->pc&0xc0000000)+(user_state->ar[0]&0x3fffffff);
    } else if (inst_type & XTHAL_BREAK_INST) {
	/* Can not debug break */
	// xt_panic();
    } else if (inst_type & XTHAL_SYSCALL_INST) {
	/* Can not debug exceptions */
	// xt_panic();
    } else if (inst_type & XTHAL_LOOP_END) {
	// xt_panic();
    } else if (inst_type & XTHAL_JUMP_INST) {
        if (inst_type & XTHAL_DEST_REG_INST) {
             return user_state->ar[inst_type>>28];
	} else if (inst_type & XTHAL_DEST_REL_INST) {
             return xthal_branch_addr((void *)user_state->pc);
        }
    } else if (inst_type & XTHAL_BRANCH_INST) {
	int branch_taken = 0;
	unsigned short inst;
	unsigned char op0, t, s, r, m, n;
	memcpy(&inst, (void *)user_state->pc, 2);
#if XCHAL_HAVE_BE
	op0 = (inst&0xf000)>>12;
	t   = (inst&0x0f00)>>8;
	s   = (inst&0x00f0)>>4;
	r   = (inst&0x000f);
	m   = t&3;
	n   = t>>2;
#else
	op0 = (inst&0x000f);
	t   = (inst&0x00f0)>>4;
	s   = (inst&0x0f00)>>8;
	r   = (inst&0xf000)>>12;
	m   = t>>2;
	n   = t&3;
#endif
	if (inst_type &XTHAL_16_BIT_INST) {
#if XCHAL_HAVE_BE
	    if (inst&0x400)	/* BNEZ.N */
		branch_taken = (user_state->ar[(inst>>4)&0xf]!=0);
	    else		/* BEQZ.N */
		branch_taken = (user_state->ar[(inst>>4)&0xf]==0);
#else
	    if (inst&0x40)	/* BNEZ.N */
		branch_taken = (user_state->ar[(inst>>8)&0xf]!=0);
	    else		/* BEQZ.N */
		branch_taken = (user_state->ar[(inst>>8)&0xf]==0);
#endif
	}
	if (op0==0x6) {
	    if (n==1) {
		if (m==0) {		/* BEQZ */
		    branch_taken = (user_state->ar[s]==0);
		} else if (m==1) {	/* BNEZ */
		    branch_taken = (user_state->ar[s]!=0);
		} else if (m==2) {	/* BLTZ */
		    branch_taken = (((int)user_state->ar[s])<0);
		} else if (m==3) {	/* BGEZ */
		    branch_taken = (((int)user_state->ar[s])>=0);
		}
	    } else if (n==2) {
		int b4const[16] =
		    { -1, 1, 2, 3, 4, 5, 6, 7,
		      8, 10, 12, 16, 32, 62, 128, 256 };
		if (m==0) {		/* BEQI */
		    branch_taken = (user_state->ar[s]==b4const[r]);
		} else if (m==1) {	/* BNEI */
		    branch_taken = (user_state->ar[s]!=b4const[r]);
		} else if (m==2) {	/* BLTI */
		    branch_taken = (((int)user_state->ar[s])<b4const[r]);
		} else if (m==3) {	/* BGEI */
		    branch_taken = (((int)user_state->ar[s])>=b4const[r]);
		}
	    } else if (n==3) {
		int b4constu[16] =
		    { 32768, 65536, 2, 3, 4, 5, 6, 7,
		      8, 10, 12, 16, 32, 62, 128, 256 };
		if (m==2) {		/* BLTUI */
		    branch_taken = (user_state->ar[s]<b4constu[r]);
		} else if (m==3) {	/* BGEUI */
		    branch_taken = (user_state->ar[s]>=b4constu[r]);
		}
	    }
	} else if (op0==0x7) {
	    if (r==0) {			/* BNONE */
		branch_taken = ((user_state->ar[s]&user_state->ar[t])==0);
	    } else if (r==1) {		/* BEQ */
		branch_taken = (user_state->ar[s]==user_state->ar[t]);
	    } else if (r==2) {		/* BLT */
		branch_taken = ((int)user_state->ar[s]<(int)user_state->ar[t]);
	    } else if (r==3) {		/* BLTU */
		branch_taken = (user_state->ar[s]<user_state->ar[t]);
	    } else if (r==4) {		/* BALL */
		branch_taken = (((~user_state->ar[s])&user_state->ar[t])==0);
	    } else if (r==5) {		/* BBC */
#if XCHAL_HAVE_BE
		branch_taken = ((user_state->ar[s]&(0x80000000>>user_state->ar[t]))==0);
	    } else if (r==6) {		/* BBCI */
		branch_taken = ((user_state->ar[s]&(0x80000000>>t))==0);
	    } else if (r==7) {		/* BBCI */
		branch_taken = ((user_state->ar[s]&(0x80000000>>(t+16)))==0);
#else
		branch_taken = ((user_state->ar[s]&(1<<user_state->ar[t]))==0);
	    } else if (r==6) {		/* BBCI */
		branch_taken = ((user_state->ar[s]&(1<<t))==0);
	    } else if (r==7) {		/* BBCI */
		branch_taken = ((user_state->ar[s]&(1<<(t+16)))==0);
#endif
	    } else if (r==8) {		/* BANY */
		branch_taken = ((user_state->ar[s]&user_state->ar[t])!=0);
	    } else if (r==9) {		/* BNE */
		branch_taken = (user_state->ar[s]!=user_state->ar[t]);
	    } else if (r==10) {		/* BGE */
		branch_taken = ((int)user_state->ar[s]>=(int)user_state->ar[t]);
	    } else if (r==11) {		/* BGEU */
		branch_taken = (user_state->ar[s]>=user_state->ar[t]);
	    } else if (r==12) {		/* BNALL */
		branch_taken = (((~user_state->ar[s])&user_state->ar[t])!=0);
	    } else if (r==13) {		/* BBS */
#if XCHAL_HAVE_BE
		branch_taken = ((user_state->ar[s]&(0x80000000>>user_state->ar[t]))!=0);
	    } else if (r==14) {		/* BBSI */
		branch_taken = ((user_state->ar[s]&(0x80000000>>t))!=0);
	    } else if (r==15) {		/* BBSI */
		branch_taken = ((user_state->ar[s]&(0x80000000>>(t+16)))!=0);
#else
		branch_taken = ((user_state->ar[s]&(1<<user_state->ar[t]))!=0);
	    } else if (r==14) {		/* BBSI */
		branch_taken = ((user_state->ar[s]&(1<<t))!=0);
	    } else if (r==15) {		/* BBSI */
		branch_taken = ((user_state->ar[s]&(1<<(t+16)))!=0);
#endif
	    }
	}
	if (branch_taken) {
	    if (inst_type & XTHAL_DEST_REG_INST) {
        	return user_state->ar[inst_type>>24];
	    } else if (inst_type & XTHAL_DEST_REL_INST) {
        	return xthal_branch_addr((void *)user_state->pc);
	    }
	}
#if XCHAL_HAVE_LOOPS
	else if (user_state->lcount && (npc==user_state->lend))
	    return user_state->lbeg;
#endif
    }
    return npc;
}

#endif /* INCLUDE_DEPRECATED_HAL_DEBUG_CODE */

