//
// interrupts.c - interrupts related constants and functions
//
// $Id: //depot/rel/Eaglenest/Xtensa/OS/hal/interrupts.c#1 $

// Copyright (c) 2002-2004 Tensilica Inc.
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

#include <xtensa/config/core.h>
#include <xtensa/config/specreg.h>

#if XCHAL_HAVE_INTERRUPTS

/*  For internal use by the HAL:  */
// static void xthal_vpri_lock(void);
// static void xthal_vpri_unlock(void);
extern void xthal_vpri_lock(void);
extern void xthal_vpri_unlock(void);


/*
 *  Definitions:
 *
 *	Virtual interrupt level = 0 .. 0xFF
 *
 *  ...
 */

#define XTHAL_DEFAULT_SOFTPRI	4	/* default software priority (range 0..15) */
					/* IMPORTANT: if you change this, you also
					   need to update the initial resolvemap[]
					   value below... */

/*
 *  Macros to convert between:
 *	intlevel (0..15) and software priority within an intlevel (0..15)
 *  and
 *	virtual interrupt priority (0..0xFF), which is a combination of the above two.
 */
#define XTHAL_VPRI_INTLEVEL(vpri)	(((vpri) >> 4) & 0xF)
#define XTHAL_VPRI_SOFTPRI(vpri)	((vpri) & 0xF)
#define XTHAL_VPRI(intlevel,softpri)	((((intlevel)&0xF)<<4)|((softpri)&0xF))


/*
 *  Virtual priority management data structures.
 *  This structure is instantiated as Xthal_vpri_state (below).
 *
 *  IMPORTANT:  if you change anything in this structure,
 *		you must accordingly change structure offsets
 *		defined in int_asm.S .
 *
 *  IMPORTANT:  the worst-case offset of the resolvemap[] field is 976 bytes
 *		(0x10 + 0x40*15), which is accessed in int_asm.S at a further
 *		offset of 8*4==32 for a total offset of 1008, very close
 *		to l32i's offset limit of 1020.  So you can't push it much
 *		further.
 *
 *  [INTERNAL NOTE:  There might be a trick that will save 64 bytes,
 *	if really needed, by trimming 15 word entries from the start
 *	of enablemap[] ...  -MG]
 */
typedef struct XtHalVPriState {
    /*
     *  Current virtual interrupt priority (0x0F .. 0xFF)
     *  (or actually, 0x0F .. XCHAL_NUM_INTLEVELS*0x10+0x0F).
     *  Virtual priorities 0x00 to 0x0E are mapped to 0x0F (they're all
     *  equivalent, because there's no such thing as a level 0 interrupt),
     *  which may help optimize the size of enablemap[] in the future.
     *  Virtual priorities above XCHAL_NUM_INTLEVELS*0x10+0x0F are
     *  mapped to XCHAL_NUM_INTLEVELS*0x10+0x0F, which is equivalent.
     *
     *  NOTE:	this variable is actually part of the processor context,
     *		which means (for most OSes) that it must be saved
     *		in the task control block along with other register state.
     */
    unsigned char	vpri;		// current virtual interrupt priority (0x0F..0xFF)
    unsigned char	locklevel;	// real interrupt level used to get exclusive
    					// access to this structure; MUST be at least one (1)
    unsigned char	lockvpri;	// virtual interrupt level used to get exclusive
    					// access to this structure; MUST be XTHAL_VPRI(locklevel,15)
					// (so it's at least 0x1F); placed here for efficiency
    unsigned char	pad0;		// (alignment padding, unused)

    unsigned	enabled;	// mask of which interrupts are enabled, regardless of level
				// (level masking is applied on top of this)

    unsigned	lockmask;	// (unused?) INTENABLE value used to lock out
				// interrupts for exclusive access to this structure

    unsigned	pad1;		// (alignment padding, unused)

    /*
     *  For each virtual interrupt priority, this array provides the
     *  bitmask of interrupts of greater virtual priority
     *  (ie. the set of interrupts to enable at that virtual priority,
     *   if all interrupts were enabled in field 'enabled').
     */
    unsigned	enablemap[XCHAL_NUM_INTLEVELS+1][16];

    /*
     * Table entries for intlevel 'i' are bitmasks defined as follows,
     * with map == Xthal_vpri_resolvemap[i-1]:
     *	map[8+(x=0)]          = ints at pri x + 8..15 (8-15)
     *	map[4+(x=0,8)]        = ints at pri x + 4..7  (4-7,12-15)
     *	map[2+(x=0,4,8,12)]   = ints at pri x + 2..3  (2-3,6-7,10-11,14-15)
     *	map[1+(x=0,2..12,14)] = ints at pri x + 1     (1,3,5,7,9,11,13,15)
     *	map[0]                = 0  (unused; for alignment)
     */
    unsigned	resolvemap[XCHAL_NUM_INTLEVELS][16];

} XtHalVPriState;


extern XtHalVPriState	Xthal_vpri_state;
extern unsigned char	Xthal_int_vpri[32];
extern XtHalVoidFunc *	Xthal_tram_trigger_fn;

extern void		xthal_null_func(void);

/*  Shorthand for structure members:  */
#define Xthal_vpri_level	Xthal_vpri_state.vpri
#define Xthal_vpri_locklevel	Xthal_vpri_state.locklevel
#define Xthal_vpri_lockvpri	Xthal_vpri_state.lockvpri
#define Xthal_vpri_enabled	Xthal_vpri_state.enabled
#define Xthal_vpri_lockmask	Xthal_vpri_state.lockmask	// unused?
#define Xthal_vpri_enablemap	Xthal_vpri_state.enablemap
#define Xthal_vpri_resolvemap	Xthal_vpri_state.resolvemap
#if 0
Combined refs:
	- enablemap, vpri, enabled		(xthal_set_vpri[_nw])
	- enablemap, vpri, enabled, resolvemap	(xthal_get_intpending_nw)
	- enablemap, vpri, enabled, locklevel	(xthal_vpri_lock)
	- enablemap, vpri, enabled		(xthal_vpri_unlock)
#endif

#endif /* XCHAL_HAVE_INTERRUPTS */



#if   defined(__SPLIT__num_intlevels)

// the number of interrupt levels
const unsigned char Xthal_num_intlevels = XCHAL_NUM_INTLEVELS;

#endif
#if defined(__SPLIT__num_interrupts)

// the number of interrupts
const unsigned char Xthal_num_interrupts = XCHAL_NUM_INTERRUPTS;

#endif
#if defined(__SPLIT__excm_level)

// the highest level of interrupts masked by PS.EXCM (if XEA2)
const unsigned char Xthal_excm_level = XCHAL_EXCM_LEVEL;

#endif
#if defined(__SPLIT__intlevel_mask)

// mask of interrupts at each intlevel
const unsigned Xthal_intlevel_mask[16] = { 
    XCHAL_INTLEVEL_MASKS
};


#endif
#if defined(__SPLIT__intlevel_andbelow_mask)

// mask for level 1 to N interrupts
const unsigned Xthal_intlevel_andbelow_mask[16] = { 
    XCHAL_INTLEVEL_ANDBELOW_MASKS
};


#endif
#if defined(__SPLIT__intlevel)

// level per interrupt
const unsigned char Xthal_intlevel[32] = { 
    XCHAL_INT_LEVELS
};


#endif
#if defined(__SPLIT__inttype)

// type of each interrupt
const unsigned char Xthal_inttype[32] = {
    XCHAL_INT_TYPES
};


#endif
#if defined(__SPLIT__inttype_mask)

const unsigned Xthal_inttype_mask[XTHAL_MAX_INTTYPES] = {
    XCHAL_INTTYPE_MASKS
};


#endif
#if defined(__SPLIT__timer_interrupt)

// interrupts assigned to each timer (CCOMPARE0 to CCOMPARE3), -1 if unassigned
const int Xthal_timer_interrupt[XTHAL_MAX_TIMERS] = { 
    XCHAL_TIMER_INTERRUPTS
};


#endif
#if defined(__SPLIT__vpri)

#if XCHAL_HAVE_INTERRUPTS

/*
 *  Note:  this structure changes dynamically at run-time,
 *  but is initialized here for efficiency and simplicity,
 *  according to configuration.
 */
XtHalVPriState  Xthal_vpri_state = {
    0x00,	/* vpri */
    1,		/* locklevel */
    0x1F,	/* lockvpri */
    0,		/* pad0 */
    0x00000000,	/* enabled */
    0x00000000,	/* lockmask (unused?) */
    0,		/* pad1 */

#define DEFAULT_ENABLEMAP(levela,levelb)	\
     { (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI > 0 ? levela : levelb)), \
       (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI > 1 ? levela : levelb)), \
       (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI > 2 ? levela : levelb)), \
       (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI > 3 ? levela : levelb)), \
       (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI > 4 ? levela : levelb)), \
       (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI > 5 ? levela : levelb)), \
       (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI > 6 ? levela : levelb)), \
       (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI > 7 ? levela : levelb)), \
       (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI > 8 ? levela : levelb)), \
       (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI > 9 ? levela : levelb)), \
       (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI >10 ? levela : levelb)), \
       (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI >11 ? levela : levelb)), \
       (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI >12 ? levela : levelb)), \
       (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI >13 ? levela : levelb)), \
       (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI >14 ? levela : levelb)), \
       (XCHAL_INTLEVEL15_ANDBELOW_MASK & ~(XTHAL_DEFAULT_SOFTPRI >15 ? levela : levelb)) }

    /*  Xthal_vpri_enablemap[XCHAL_NUM_INTLEVELS+1][16]:  */
    {
	DEFAULT_ENABLEMAP(XCHAL_INTLEVEL0_ANDBELOW_MASK,XCHAL_INTLEVEL0_ANDBELOW_MASK),
#if XCHAL_NUM_INTLEVELS >= 1
	DEFAULT_ENABLEMAP(XCHAL_INTLEVEL0_ANDBELOW_MASK,XCHAL_INTLEVEL1_ANDBELOW_MASK),
#endif
#if XCHAL_NUM_INTLEVELS >= 2
	DEFAULT_ENABLEMAP(XCHAL_INTLEVEL1_ANDBELOW_MASK,XCHAL_INTLEVEL2_ANDBELOW_MASK),
#endif
#if XCHAL_NUM_INTLEVELS >= 3
	DEFAULT_ENABLEMAP(XCHAL_INTLEVEL2_ANDBELOW_MASK,XCHAL_INTLEVEL3_ANDBELOW_MASK),
#endif
#if XCHAL_NUM_INTLEVELS >= 4
	DEFAULT_ENABLEMAP(XCHAL_INTLEVEL3_ANDBELOW_MASK,XCHAL_INTLEVEL4_ANDBELOW_MASK),
#endif
#if XCHAL_NUM_INTLEVELS >= 5
	DEFAULT_ENABLEMAP(XCHAL_INTLEVEL4_ANDBELOW_MASK,XCHAL_INTLEVEL5_ANDBELOW_MASK),
#endif
#if XCHAL_NUM_INTLEVELS >= 6
	DEFAULT_ENABLEMAP(XCHAL_INTLEVEL5_ANDBELOW_MASK,XCHAL_INTLEVEL6_ANDBELOW_MASK),
#endif
#if XCHAL_NUM_INTLEVELS >= 7
# error	Interrupt levels greater than 6 not currently supported in the HAL interrupt routines.
#endif
    },

    /*  Xthal_vpri_resolvemap[XCHAL_NUM_INTLEVELS][16]:  */
    {
#if XCHAL_NUM_INTLEVELS >= 1	/* set for default soft priority of 4: */
     {0,0,0,0, XCHAL_INTLEVEL1_MASK,0,0,0, 0,0,0,0, 0,0,0,0},
#endif
#if XCHAL_NUM_INTLEVELS >= 2	/* set for default soft priority of 4: */
     {0,0,0,0, XCHAL_INTLEVEL2_MASK,0,0,0, 0,0,0,0, 0,0,0,0},
#endif
#if XCHAL_NUM_INTLEVELS >= 3	/* set for default soft priority of 4: */
     {0,0,0,0, XCHAL_INTLEVEL3_MASK,0,0,0, 0,0,0,0, 0,0,0,0},
#endif
#if XCHAL_NUM_INTLEVELS >= 4	/* set for default soft priority of 4: */
     {0,0,0,0, XCHAL_INTLEVEL4_MASK,0,0,0, 0,0,0,0, 0,0,0,0},
#endif
#if XCHAL_NUM_INTLEVELS >= 5	/* set for default soft priority of 4: */
     {0,0,0,0, XCHAL_INTLEVEL5_MASK,0,0,0, 0,0,0,0, 0,0,0,0},
#endif
#if XCHAL_NUM_INTLEVELS >= 6	/* set for default soft priority of 4: */
     {0,0,0,0, XCHAL_INTLEVEL6_MASK,0,0,0, 0,0,0,0, 0,0,0,0},
#endif
#if XCHAL_NUM_INTLEVELS >= 7	/* set for default soft priority of 4: */
# error	Interrupt levels greater than 6 not currently supported in the HAL interrupt routines.
#endif
    }

};


/*
 *  Virtual (software) priority (0x00..0xFF) of each interrupt.
 *  This isn't referenced by assembler.
 */
unsigned char	Xthal_int_vpri[32] = {
#define DEFAULT_INTVPRI(level)	(level ? ((level << 4) | XTHAL_DEFAULT_SOFTPRI) : 0)
    DEFAULT_INTVPRI( XCHAL_INT0_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT1_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT2_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT3_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT4_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT5_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT6_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT7_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT8_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT9_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT10_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT11_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT12_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT13_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT14_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT15_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT16_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT17_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT18_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT19_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT20_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT21_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT22_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT23_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT24_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT25_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT26_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT27_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT28_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT29_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT30_LEVEL ),
    DEFAULT_INTVPRI( XCHAL_INT31_LEVEL )
};


#if 0
/*
 *  A number of things may have already been written not calling
 *  this function, so it isn't straightforward to start requiring it:
 */
void xthal_vpri_init( int default_vpri )
{
    int i, j;

    Xthal_vpri_level = 0;		/* vpri */
    Xthal_vpri_locklevel = 1;		/* locklevel */
    Xthal_vpri_lockvpri = 0x1F;		/* lockvpri */
    Xthal_vpri_enabled = 0x00000000;	/* enabled */
    Xthal_vpri_lockmask = 0x00000000;	/* lockmask (unused?) */
    for( i = 0; i < XCHAL_NUM_INTLEVELS; i++ ) {
	for( j = 0; j < 16; j++ )
	    Xthal_vpri_enablemap[i][j] = XCHAL_INTLEVEL15_ANDBELOW_MASK
		    & ~Xthal_intlevel_andbelow_mask[i - (j < default_vpri && i > 0)];
    }
    for( i = 1; i < XCHAL_NUM_INTLEVELS; i++ ) {
	for( j = 0; j < 16; j++ )
	    Xthal_vpri_resolvemap[i-1][j] = 0;
	if( (default_vpri & 1) != 0 )
	    Xthal_vpri_resolvemap[i-1][default_vpri & 0xF] |= Xthal_intlevel_mask[i];
	if( (default_vpri & 2) != 0 )
	    Xthal_vpri_resolvemap[i-1][default_vpri & 0xE] |= Xthal_intlevel_mask[i];
	if( (default_vpri & 4) != 0 )
	    Xthal_vpri_resolvemap[i-1][default_vpri & 0xC] |= Xthal_intlevel_mask[i];
	if( (default_vpri & 8) != 0 )
	    Xthal_vpri_resolvemap[i-1][default_vpri & 0x8] |= Xthal_intlevel_mask[i];
    }
    for( i = 0; i < 32; i++ )
	Xthal_int_vpri[i] = (Xthal_intlevel[i] << 4) | (default_vpri & 0xF);
}
#endif /*0*/

void xthal_null_func(void) { }
XtHalVoidFunc *Xthal_tram_trigger_fn = xthal_null_func;


#endif /* XCHAL_HAVE_INTERRUPTS */


#endif
#if defined(__SPLIT__vpri_to_intlevel)

/*
 *  xthal_vpri_to_intlevel
 *
 *  Converts a virtual interrupt priority to the closest equivalent
 *  (equal or higher) interrupt level.
 */
unsigned xthal_vpri_to_intlevel(unsigned vpri)
{
#if XCHAL_HAVE_INTERRUPTS
    return( XTHAL_VPRI_INTLEVEL( vpri ) );
#else
    return( vpri );
#endif
}

#endif
#if defined(__SPLIT__intlevel_to_vpri)

/*
 *  xthal_intlevel_to_vpri
 *
 *  Converts an interrupt level to a virtual interrupt priority.
 */
unsigned xthal_intlevel_to_vpri(unsigned intlevel)
{
#if XCHAL_HAVE_INTERRUPTS
    return( XTHAL_VPRI( intlevel, 0xF ) );
#else
    return( intlevel );
#endif
}


#endif
#if defined(__SPLIT__vpri_int_enable)

/*
 *  xthal_int_enable
 *
 *  Enables given set of interrupts, and returns previous enabled-state of these interrupts.
 */
unsigned xthal_int_enable(unsigned mask)
{
#if XCHAL_HAVE_INTERRUPTS
    unsigned prev_enabled, syncmask;

    xthal_vpri_lock();
    prev_enabled = Xthal_vpri_enabled | Xthal_tram_enabled;

    /*  Figure out which bits must go in Xthal_tram_enabled:  */
    syncmask = (mask & Xthal_tram_pending & Xthal_tram_sync);
    if( syncmask != 0 ) {
	Xthal_tram_enabled |= syncmask;
	mask &= ~syncmask;
	/*
	 *  If we are re-enabling a pending trampolined interrupt,
	 *  there is a possibility that the level-1 software interrupt
	 *  is no longer pending, having already occurred (without processing
	 *  the trampoline because it was disabled).  So we have to
	 *  ensure that the level-1 software interrupt used for trampolining
	 *  is pending.
	 *  We let the BSP do this rather than the HAL, because it could
	 *  potentially use an external level-1 interrupt to trampoline
	 *  (if proper hardware was available) rather than a software interrupt.
	 */
	(*Xthal_tram_trigger_fn)();
    }
    /*  The rest go in the global enabled mask:  */
    Xthal_vpri_enabled |= mask;

    xthal_vpri_unlock();	/* update INTENABLE as per current vpri */
    return( prev_enabled );

#else /* XCHAL_HAVE_INTERRUPTS */
    return( 0 );
#endif /* XCHAL_HAVE_INTERRUPTS */
}

#endif
#if defined(__SPLIT__vpri_int_disable)

/*
 *  xthal_int_disable
 *
 *  Disables given set of interrupts, and returns previous enabled-state of these interrupts.
 */
unsigned xthal_int_disable(unsigned mask)
{
#if XCHAL_HAVE_INTERRUPTS
    unsigned prev_enabled;

    xthal_vpri_lock();
    prev_enabled = Xthal_vpri_enabled | Xthal_tram_enabled;
    Xthal_vpri_enabled &= ~mask;
    Xthal_tram_enabled &= ~mask;
    xthal_vpri_unlock();	/* update INTENABLE as per current vpri */
    return( prev_enabled );
#else
    return( 0 );
#endif
}


#endif
#if defined(__SPLIT__set_vpri_locklevel)

void  xthal_set_vpri_locklevel(unsigned intlevel)
{
#if XCHAL_HAVE_INTERRUPTS
    if( intlevel < 1 )
	intlevel = 1;
    else if( intlevel > XCHAL_NUM_INTLEVELS )
	intlevel = XCHAL_NUM_INTLEVELS;
    Xthal_vpri_state.locklevel = intlevel;
    Xthal_vpri_state.lockvpri = XTHAL_VPRI(intlevel, 15);
#endif
}

#endif
#if defined(__SPLIT__get_vpri_locklevel)

unsigned  xthal_get_vpri_locklevel(void)
{
#if XCHAL_HAVE_INTERRUPTS
    return( Xthal_vpri_state.locklevel );
#else
    return( 1 );	/* must return at least 1, some OSes assume this */
#endif
}


#endif
#if defined(__SPLIT__set_int_vpri)

/*
 *  xthal_set_int_vpri   (was intSetL1Pri)
 *
 *  Set the virtual (software) priority of an interrupt.
 *  Note:  the intlevel of an interrupt CANNOT be changed -- this is
 *  set in hardware according to the core configuration file.
 *
 *	intnum		interrupt number (0..31)
 *	vpri		virtual interrupt priority (0..15, or intlevel*16+(0..15) )
 */
int  xthal_set_int_vpri(int intnum, int vpri)
{
#if XCHAL_HAVE_INTERRUPTS
    unsigned  mask, maskoff, basepri, prevpri, intlevel, *maskp, i;

    /*
     *  Verify parameters:
     */
    if( (unsigned)intnum >= XCHAL_NUM_INTERRUPTS || (unsigned)vpri > 0xFF )
	return( 0 );		/* error: bad parameter(s) */
    /*
     *  If requested priority specifies an intlevel, it must match that
     *  of the interrupt specified; otherwise (0..15) the proper intlevel of
     *  the specified interrupt is assumed, and added to the parameter:
     */
    intlevel = Xthal_intlevel[intnum];	/* intnum's intlevel */
    if( intlevel == 0 || intlevel > XCHAL_NUM_INTLEVELS )
    	return( 0 );		/* error: no support for setting priority of NMI etc. */
    basepri = intlevel << 4;		/* intnum's base soft-pri. */
    if( vpri > 0x0F ) {			/* intlevel portion given? */
	if( (vpri & 0xF0) != basepri )	/* then it must be correct */
	    return( 0 );		/* error: intlevel mismatch */
	vpri &= 0x0F;			/* remove it */
    }

    mask = 1L << intnum;

    /*
     *  Lock interrupts during virtual priority data structure updates:
     */
    xthal_vpri_lock();

    /*
     *  Update virtual priority of 'intnum':
     */
    prevpri = Xthal_int_vpri[intnum];	/* save for return value */
    Xthal_int_vpri[intnum] = basepri | vpri;
    /*  This interrupt must only be enabled at virtual priorities lower than its own:  */
    for( i = 0; i < vpri; i++ )
	Xthal_vpri_enablemap[0][basepri++] |= mask;
    maskoff = ~mask;
    for( ; i <= 0x0F; i++ )
	Xthal_vpri_enablemap[0][basepri++] &= maskoff;

    /*
     *  Update the prioritization table used to resolve priorities by binary search:
     */
    /*  Remove interrupt <intnum> from prioritization table:  */
    maskp = Xthal_vpri_resolvemap[intlevel-1];
    for (i=0; i<16; i++)
	maskp[i] &= maskoff;
    /*  Add interrupt <intnum> to prioritization table at its (new) given priority:  */
    if( vpri & 0x1 )
	maskp[vpri] |= mask;
    if( vpri & 0x2 )
	maskp[vpri & 0xE] |= mask;
    if( vpri & 0x4 )
	maskp[vpri & 0xC] |= mask;
    if( vpri & 0x8 )
	maskp[vpri & 0x8] |= mask;

    /*
     *  Unlock interrupts (back to current level) and update INTENABLE:
     */
    xthal_vpri_unlock();

    return( prevpri );
#else /* XCHAL_HAVE_INTERRUPTS */
    return( 0 );
#endif /* XCHAL_HAVE_INTERRUPTS */
} /* xthal_set_int_vpri */


#endif
#if defined(__SPLIT__get_int_vpri)

int	xthal_get_int_vpri(int intnum)
{
#if XCHAL_HAVE_INTERRUPTS
    if( (unsigned)intnum >= XCHAL_NUM_INTERRUPTS )
	return( 0 );		/* error: bad parameter */
    return( Xthal_int_vpri[intnum] );
#else
    return( 0 );
#endif
}



#endif
#if defined(__SPLIT__trampolines)


	/*
	SUPPORT FOR TRAMPOLINES

	NOTE:  trampolining is a special case.
	There are two ways (defined here) to trampoline down
	from a high-level interrupt to a level-one interrupt.

	a)  Synchronous (restrained) trampolining.
	    Trampolining without clearing the high-level interrupt,
	    letting the level-one interrupt handler clear the
	    source of the interrupt.
	    Here the high-level interrupt must be kept disabled
	    while trampolining down, and re-enabled after the
	    level-one interrupt handler completes.
	    This is what one might do to "convert" a high-level
	    interrupt into a level-one interrupt.
	    The high-level interrupt handler code can be generic.
	    [One could argue this type of trampolining isn't required,
	     which may? be true...]
	b)  Asynchronous (free) trampolining.
	    Trampolining when clearing the high-level interrupt
	    right away in the high-level interrupt handler.
	    Here the high-level interrupt is allowed to remain
	    enabled while trampolining occurs.  This is very
	    useful when some processing must occur with low
	    latency, but the rest of the processing can occur
	    at lower (eg. level-one) priority.  It is particularly
	    useful when the lower-priority processing occurs
	    for only some of the high-level interrupts.
	    Of course this requires custom assembler code to
	    handle the high-level interrupt and clear the source
	    of the interrupt, so the high-level interrupt handler
	    cannot be generic (as opposed to synchronous trampolining).

	In both cases, a level-one software interrupt is used
	for trampolining (one could also trampoline from level
	m to n, m > n, n > 1, but that isn't nearly as useful;
	it's generally the ability to execute C code and
	to process exceptions that is sought after).

	Default trampolining support is currently implemented as follows.

	Trampoline handler:

	A high-level interrupt is considered enabled if *either*
	its INTENABLE bit or its xt_tram_ints bit is set
	(note that both should never be set at the same time).

	 */


/*  These are described in xtensa/hal.h (assumed initialized to zero, in BSS):  */
unsigned Xthal_tram_pending;
unsigned Xthal_tram_enabled;
unsigned Xthal_tram_sync;



XtHalVoidFunc* xthal_set_tram_trigger_func( XtHalVoidFunc *trigger_fn )
{
#if XCHAL_HAVE_INTERRUPTS
    XtHalVoidFunc *fn;

    fn = Xthal_tram_trigger_fn;
    Xthal_tram_trigger_fn = trigger_fn;
    return( fn );
#else
    (void)trigger_fn;
    return( 0 );
#endif
}


/*
 *  xthal_tram_set_sync
 *
 *  Configure type of trampoline for a high-level interrupt.
 *  By default any trampoline is asynchronous, this need only
 *  be called to tell the Core HAL that a high-level interrupt
 *  will be using synchronous trampolining (down to a level-1 interrupt).
 *
 *	intnum		interrupt number (0 .. 31)
 *	sync		0 = async, 1 = synchronous
 *
 *  Returns previous sync state of interrupt (0 or 1)
 *  or -1 if invalid interrupt number provided.
 */
int  xthal_tram_set_sync( int intnum, int sync )
{
#if XCHAL_HAVE_INTERRUPTS
    unsigned mask;
    int prev;

    if( (unsigned)intnum >= XCHAL_NUM_INTERRUPTS )
	return( -1 );
    mask = 1L << intnum;
    prev = ((Xthal_tram_sync & mask) != 0);
    if( sync )
	Xthal_tram_sync |= mask;
    else
	Xthal_tram_sync &= ~mask;
    return( prev );
#else /* XCHAL_HAVE_INTERRUPTS */
    return( 0 );
#endif /* XCHAL_HAVE_INTERRUPTS */
}


/*
 *  xthal_tram_pending_to_service
 *
 *  This is called by the trampoline interrupt handler
 *  (eg. by a level-one software interrupt handler)
 *  to obtain the bitmask of high-level interrupts
 *  that it must service.
 *  Returns that bitmask (note: this can sometimes be zero,
 *  eg. if currently executing level-one code disables the high-level
 *  interrupt before the trampoline handler has a chance to run).
 *
 *  This call automatically clears the trampoline pending
 *  bits for the interrupts in the returned mask.
 *  So the caller *must* process all interrupts that have
 *  a corresponding bit set if the value returned by this function
 *  (otherwise those interrupts may likely be lost).
 *
 *  This function should be called with level-one interrupts disabled
 *  (via INTENABLE; can't be via PS.INTLEVEL because this is C code).
 */
unsigned  xthal_tram_pending_to_service( void )
{
#if XCHAL_HAVE_INTERRUPTS
    unsigned  service_mask;

    service_mask = ( Xthal_tram_pending
			& (Xthal_vpri_enabled | Xthal_tram_enabled) ) ;

    /*
     *  Clear trampoline pending bits.
     *  Each bit must be cleared *before* processing of the corresponding
     *  interrupt occurs, to avoid missing interrupts.
     *  Here we just clear all bits for simplicity and convenience.
     */
    Xthal_tram_pending &= ~service_mask;

    return( service_mask );
#else /* XCHAL_HAVE_INTERRUPTS */
    return( 0 );
#endif /* XCHAL_HAVE_INTERRUPTS */
}

/*
 *  xthal_tram_done
 *
 *  This is called by the trampoline interrupt handler
 *  (eg. by a level-one software interrupt handler)
 *  to indicate that processing of a trampolined interrupt
 *  (eg. one or more of the bits it received from
 *   xthal_tram_acknowledge()) has completed.
 *
 *  For asynchronously trampolined interrupt(s), there is nothing to do.
 *  For synchronously trampolined interrupt(s), the high-level
 *  interrupt(s) must be re-enabled (presumably the level-one
 *  interrupt handler that just completed has cleared the source
 *  of the high-level interrupt).
 *
 *  This function should be called with level-one interrupts disabled
 *  (via INTENABLE; can't be via PS.INTLEVEL because this is C code).
 */
void  xthal_tram_done( unsigned serviced_mask )
{
#if XCHAL_HAVE_INTERRUPTS
    serviced_mask &= Xthal_tram_enabled;	/* sync. trampolined interrupts that completed */
    Xthal_tram_enabled &= ~serviced_mask;
    xthal_int_enable( serviced_mask );
#endif
}

#endif
#if defined(__SPLIT__deprecated)


/**********************************************************************/

#ifdef INCLUDE_DEPRECATED_HAL_CODE
/*  These definitions were present in an early beta version of the HAL and should not be used:  */
const unsigned Xthal_num_int_levels = XCHAL_NUM_INTLEVELS;
const unsigned Xthal_num_ints = XCHAL_NUM_INTERRUPTS;
__asm__(".global Xthal_int_level_mask\n"	".set Xthal_int_level_mask,       Xthal_intlevel_mask+4");
__asm__(".global Xthal_int_level1_to_n_mask\n"	".set Xthal_int_level1_to_n_mask, Xthal_intlevel_andbelow_mask+8");
/*const unsigned Xthal_int_level_mask[15] = { XCHAL_INTLEVEL_MASKS };			... minus the first entry ...*/
/*const unsigned Xthal_int_level1_to_n_mask[14] = { XCHAL_INTLEVEL_ANDBELOW_MASKS };	... minus the first two entries ...*/
const unsigned Xthal_int_level[32] = { XCHAL_INT_LEVELS };
const unsigned Xthal_int_type_edge = XCHAL_INTTYPE_MASK_EXTERN_EDGE;
const unsigned Xthal_int_type_level = XCHAL_INTTYPE_MASK_EXTERN_LEVEL;
const unsigned Xthal_int_type_timer = XCHAL_INTTYPE_MASK_TIMER;
const unsigned Xthal_int_type_software = XCHAL_INTTYPE_MASK_SOFTWARE;
#endif /* INCLUDE_DEPRECATED_HAL_CODE */


#endif /* SPLITs */

