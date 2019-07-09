/*
 * Copyright (c) 2004-2015 Cadence Design Systems Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <xtensa/config/core.h>

#if XCHAL_HAVE_MPU
#include <xtensa/core-macros.h>
#include <xtensa/hal.h>
#include <string.h>
#include <stdlib.h>

/*
 * General notes:
 * Wherever an address is represented as an unsigned, it has only the 27 most significant bits.  This is how
 * the addresses are represented in the MPU.  It has the benefit that we don't need to worry about overflow.
 *
 * The asserts in the code are ignored unless an assert handler is set (as it is during testing).
 *
 * If an assert handler is set, then the MPU map is checked for correctness after every update.
 *
 * On some configs (actually all configs right now),  the MPU entries must be aligned to the background map.
 * The constant: XCHAL_MPU_ALIGN_REQ indicates if alignment is required:
 *
 * The rules for a valid map are:
 *
 * 1) The entries' vStartAddress fields must always be in non-descending order.
 * 2) The entries' memoryType and accessRights must contain valid values
 *
 * If XCHAL_MPU_ALIGN_REQ == 1 then the following additional rules are enforced:
 * 3) If entry0's Virtual Address Start field is nonzero, then that field must equal one of the
 *    Background Map's Virtual Address Start field values if software ever intends to assert entry0's MPUENB bit.
 * 4) If entryN's MPUENB bit will ever be negated while at the same time entryN+1's MPUENB bit is asserted,
 *    then entryN+1's Virtual Address Start field must equal one of the Background Map's Virtual Address Start field values.
 *
 * The internal function are first, and the external 'xthal_' functions are at the end.
 *
 */
extern void (*_xthal_assert_handler)();
extern void xthal_write_map_raw(const xthal_MPU_entry* fg, unsigned int n);
extern void xthal_read_map_raw(const xthal_MPU_entry* fg);
extern xthal_MPU_entry _xthal_get_entry(const xthal_MPU_entry* fg, const xthal_MPU_entry* bg,
        unsigned int addr, int* infgmap);

#define MPU_ADDRESS_MASK (0xffffffff << XCHAL_MPU_ALIGN_BITS)
#define MPU_ALIGNMENT_MASK (0xffffffff - MPU_ADDRESS_MASK)

#define MPU_VSTART_CORRECTNESS_MASK  ((0x1 << (XCHAL_MPU_ALIGN_BITS)) - 1)
// Set this to 1 for more extensive internal checking / 0 for production
#define MPU_DEVELOPMENT_MODE 0

#if XCHAL_MPU_ALIGN_REQ
#define XCHAL_MPU_WORST_CASE_ENTRIES_FOR_REGION             3
#else
#define XCHAL_MPU_WORST_CASE_ENTRIES_FOR_REGION             2
#endif

/*
 * At some point it is faster to commit/invalidate the entire cache rather than going on line at a time.
 * If a region is bigger than 'CACHE_REGION_THRESHOLD' we operate on the entire cache.
 */
#if XCHAL_DCACHE_LINESIZE
#define CACHE_REGION_THRESHOLD (32 * XCHAL_DCACHE_LINESIZE / XCHAL_MPU_ALIGN)
#else
#define CACHE_REGION_THRESHOLD 0
#endif


/*
 * Normally these functions are no-ops, but the MPU test harness sets an assert handler to detect any inconsistencies in MPU
 * entries or any other unexpected internal condition.
 */
#if MPU_DEVELOPMENT_MODE
static void my_assert(int arg)
{
    if (_xthal_assert_handler && !arg)
        _xthal_assert_handler();
}

static void assert_map_valid()
{

    if (_xthal_assert_handler)
    {
        xthal_MPU_entry fg[XCHAL_MPU_ENTRIES];
        xthal_read_map(fg);
        if (xthal_check_map(fg, XCHAL_MPU_ENTRIES))
            _xthal_assert_handler();
    }
}

static void assert_attributes_equivalent(unsigned addr, const xthal_MPU_entry* initial,
        const xthal_MPU_entry* fg, const xthal_MPU_entry* bg)
{

    xthal_MPU_entry e1 = _xthal_get_entry(initial, bg, addr, 0);
    xthal_MPU_entry e2 = _xthal_get_entry(fg, bg, addr, 0);
    my_assert((XTHAL_MPU_ENTRY_GET_ACCESS(e1) == XTHAL_MPU_ENTRY_GET_ACCESS(e2)) && (XTHAL_MPU_ENTRY_GET_MEMORY_TYPE(e1) == XTHAL_MPU_ENTRY_GET_MEMORY_TYPE(e2)));
}

static void assert_maps_equivalent(const xthal_MPU_entry* initial, const xthal_MPU_entry* fg,
        const xthal_MPU_entry* bg)
{
    /* this function checks that for every address the MPU entries 'initial' result in the same attributes as the entries in 'fg'.
     * We only need to check at the addresses that appear in 'initial', 'fg', or 'bg'.
     */
    int i;
    for (i = 0; i < XCHAL_MPU_ENTRIES; i++)
    {
        assert_attributes_equivalent(XTHAL_MPU_ENTRY_GET_VSTARTADDR(initial[i]), initial, fg, bg);
        assert_attributes_equivalent(XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]), initial, fg, bg);
    }
    for (i = 0; i < XCHAL_MPU_BACKGROUND_ENTRIES; i++)
        assert_attributes_equivalent(XTHAL_MPU_ENTRY_GET_VSTARTADDR(bg[i]), initial, fg, bg);
}
#else
#define my_assert(x)
#define assert_map_valid(x)
#endif

#if 0
// These functions aren't used, but am leaving the definitions in place
// for possible future use.
static inline unsigned read_mpucfg()
{
    unsigned long tmp;
    __asm__ __volatile__("rsr.mpucfg %0\n\t"
            : "=a" (tmp));
    return tmp;
}

static inline unsigned read_mpuenb()
{
    unsigned long tmp;
    __asm__ __volatile__("rsr.mpuenb %0\n\t"
            : "=a" (tmp));
    return tmp;
}

/* This function writes the enable for the MPU entries */
static inline void write_mpuenb(unsigned v)
{
    __asm__ __volatile__("wsr.mpuenb %0\n\t"
            : : "a" (v));
}

#endif

static inline void isync()
{
    __asm__ __volatile__("isync\n\t");
}

/* This function writes the cache disable register which
 * disables the cache by 512MB registers to save power*/
static  inline void write_cacheadrdis(unsigned v)
{
    __asm__ __volatile__("wsr.cacheadrdis %0\n\t"
            : : "a" (v));
}

inline static int is_cacheable(unsigned int mt);

#if 0
static inline void read_map_entry(unsigned en_num, xthal_MPU_entry* en)
{
    unsigned as;
    unsigned at0;
    unsigned at1;
    as = en_num;
    __asm__ __volatile__("RPTLB0 %0, %1\n\t" : "+a" (at0) : "a" (as));
    __asm__ __volatile__("RPTLB1 %0, %1\n\t" : "+a" (at1) : "a" (as));
    en->as = at0;
    en->at = at1;
}
#endif

inline static int is_cacheable(unsigned int mt)
{
    return (0x180 & mt) || ((mt & 0x18) == 0x10) || ((mt & 0x30) == 0x30);
}

inline static int is_writeback(unsigned int mt)
{
    return (((0x180 & mt) && (mt & 0x11)) ||
            ((((mt & 0x18) == 0x10) || ((mt & 0x30) == 0x30)) & 0x1));
}

inline static int is_device(unsigned int mt)
{
    return ((mt & 0x1f0) == 0);
}

inline static int is_kernel_readable(int accessRights)
{
    switch (accessRights)
    {
    case XTHAL_AR_R:
    case XTHAL_AR_Rr:
    case XTHAL_AR_RX:
    case XTHAL_AR_RXrx:
    case XTHAL_AR_RW:
    case XTHAL_AR_RWX:
    case XTHAL_AR_RWr:
    case XTHAL_AR_RWrw:
    case XTHAL_AR_RWrwx:
    case XTHAL_AR_RWXrx:
    case XTHAL_AR_RWXrwx:
        return 1;
    case XTHAL_AR_NONE:
    case XTHAL_AR_Ww:
        return 0;
    default:
        return XTHAL_BAD_ACCESS_RIGHTS;
    }
}

inline static int is_kernel_writeable(int accessRights)
{
    switch (accessRights)
    {
    case XTHAL_AR_RW:
    case XTHAL_AR_RWX:
    case XTHAL_AR_RWr:
    case XTHAL_AR_RWrw:
    case XTHAL_AR_RWrwx:
    case XTHAL_AR_RWXrx:
    case XTHAL_AR_RWXrwx:
    case XTHAL_AR_Ww:
        return 1;
    case XTHAL_AR_NONE:
    case XTHAL_AR_R:
    case XTHAL_AR_Rr:
    case XTHAL_AR_RX:
    case XTHAL_AR_RXrx:
        return 0;
    default:
        return XTHAL_BAD_ACCESS_RIGHTS;
    }
}

inline static int is_kernel_executable(int accessRights)
{
    switch (accessRights)
    {
    case XTHAL_AR_RX:
    case XTHAL_AR_RXrx:
    case XTHAL_AR_RWX:
    case XTHAL_AR_RWXrx:
    case XTHAL_AR_RWXrwx:
        return 1;
    case XTHAL_AR_NONE:
    case XTHAL_AR_Ww:
    case XTHAL_AR_R:
    case XTHAL_AR_Rr:
    case XTHAL_AR_RW:
    case XTHAL_AR_RWr:
    case XTHAL_AR_RWrw:
    case XTHAL_AR_RWrwx:
        return 0;
    default:
        return XTHAL_BAD_ACCESS_RIGHTS;
    }
}

inline static int is_user_readable(int accessRights)
{
    switch (accessRights)
    {
    case XTHAL_AR_Rr:
    case XTHAL_AR_RXrx:
    case XTHAL_AR_RWr:
    case XTHAL_AR_RWrw:
    case XTHAL_AR_RWrwx:
    case XTHAL_AR_RWXrx:
    case XTHAL_AR_RWXrwx:
        return 1;
    case XTHAL_AR_R:
    case XTHAL_AR_RX:
    case XTHAL_AR_RW:
    case XTHAL_AR_RWX:
    case XTHAL_AR_NONE:
    case XTHAL_AR_Ww:
        return 0;
    default:
        return XTHAL_BAD_ACCESS_RIGHTS;
    }
}

inline static int is_user_writeable(int accessRights)
{
    switch (accessRights)
    {
    case XTHAL_AR_Ww:
    case XTHAL_AR_RWrw:
    case XTHAL_AR_RWrwx:
    case XTHAL_AR_RWXrwx:
        return 1;
    case XTHAL_AR_NONE:
    case XTHAL_AR_R:
    case XTHAL_AR_Rr:
    case XTHAL_AR_RX:
    case XTHAL_AR_RXrx:
    case XTHAL_AR_RW:
    case XTHAL_AR_RWX:
    case XTHAL_AR_RWr:
    case XTHAL_AR_RWXrx:
        return 0;
    default:
        return XTHAL_BAD_ACCESS_RIGHTS;
    }
}

inline static int is_user_executable(int accessRights)
{
    switch (accessRights)
    {
    case XTHAL_AR_RXrx:
    case XTHAL_AR_RWrwx:
    case XTHAL_AR_RWXrx:
    case XTHAL_AR_RWXrwx:
        return 1;
    case XTHAL_AR_RW:
    case XTHAL_AR_RWX:
    case XTHAL_AR_RWr:
    case XTHAL_AR_RWrw:
    case XTHAL_AR_R:
    case XTHAL_AR_Rr:
    case XTHAL_AR_RX:
    case XTHAL_AR_NONE:
    case XTHAL_AR_Ww:
        return 0;
    default:
        return XTHAL_BAD_ACCESS_RIGHTS;
    }
}

/* This function returns the map entry that is used for the address 'addr' (27msb).
 *
 */
#if defined(__SPLIT__mpu_basic)

xthal_MPU_entry _xthal_get_entry(const xthal_MPU_entry* fg, const xthal_MPU_entry* bg,
        unsigned int addr, int* infgmap)
{
    int i;
    for (i = XCHAL_MPU_ENTRIES - 1; i >= 0; i--)
    {
        if (XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]) <= addr)
        {
            if (XTHAL_MPU_ENTRY_GET_VALID(fg[i]))
            {
                if (infgmap)
                    *infgmap = 1;
                return fg[i];
            }
            else
                break;
        }
    }
    for (i = XCHAL_MPU_BACKGROUND_ENTRIES - 1; i >= 0; i--)
    {
        if (XTHAL_MPU_ENTRY_GET_VSTARTADDR(bg[i]) <= addr)
        {
            if (infgmap)
                *infgmap = 0;
            return bg[i];
        }
    }
    return bg[0]; // never reached ... just to get rid of compilation warning
}

/* returns true if the supplied address (27msb) is in the background map. */
int _xthal_in_bgmap(unsigned int address, const xthal_MPU_entry* bg)
{
    int i;
    for (i = 0; i < XCHAL_MPU_BACKGROUND_ENTRIES; i++)
        if (XTHAL_MPU_ENTRY_GET_VSTARTADDR(bg[i]) == address)
            return 1;
    return 0;
}

#endif

#if defined(__SPLIT__mpu_attributes)

/* This function updates the map entry as well as internal duplicate of the map
 * state in fg.  The assumption is that reading map entries could be somewhat
 * expensive in some situations so we are keeping a copy of the map in memory when
 * doing extensive map manipulations.
 */
static void write_map_entry(xthal_MPU_entry* fg, unsigned en_num, xthal_MPU_entry en)
{
    en.at = (en.at & 0xffffffe0) | (en_num & 0x1f);
    xthal_mpu_set_entry(en);
    assert_map_valid();
    fg[en_num] = en;
}

static void move_map_down(xthal_MPU_entry* fg, int dup, int idx)
{
    /* moves the map entry list down one (leaving duplicate entries at idx and idx+1.  This function assumes that the last
     * entry is invalid ... call MUST check this
     */
    unsigned int i;
    for (i = dup; i > idx; i--)
    {
        write_map_entry(fg, i, fg[i - 1]);
    }
}

static void move_map_up(xthal_MPU_entry* fg, int dup, int idx)
{
    /* moves the map entry list up one (leaving duplicate entries at idx and idx-1, removing the entry at dup
     */
    int i;
    for (i = dup; i < idx - 1; i++)
    {
        write_map_entry(fg, i, fg[i + 1]);
    }
}

static int bubble_free_to_ip(xthal_MPU_entry* fg, int ip, int required)
{
    /* This function shuffles the entries in the MPU to get at least 'required' free entries at
     * the insertion point 'ip'.  This function returns the new insertion point (after all the shuffling).
     */
    int i;
    int rv = ip;
    if (required < 1)
        return ip;
    my_assert(required <= XCHAL_MPU_ENTRIES);
    /* first we search for duplicate or unused entries at an index less than 'ip'.  We start looking at ip-1
     * (rather than 0) to minimize the number of shuffles required.
     */
    for (i = ip - 2; i >= 0 && required;)
    {
        if (XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]) == XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i + 1]))
        {
            move_map_up(fg, i, ip);
            rv--;
            required--;
        }
        i--;
    }
    // if there are any invalid entries at top of the map, we can remove them to make space
    while (required)
    {
        if (!XTHAL_MPU_ENTRY_GET_VALID(fg[0]))
        {
            move_map_up(fg, 0, ip);
            rv--;
            required--;
        }
        else
            break;
    }
    /* If there are not enough unneeded entries at indexes less than ip, then we search at indexes > ip.
     * We start the search at ip+1 and move down, again to minimize the number of shuffles required.
     */

    for (i = ip + 1; i < XCHAL_MPU_ENTRIES && required;)
    {
        if (XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]) == XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i - 1]))
        {
            move_map_down(fg, i, ip);
            required--;
        }
        else
            i++;
    }
    my_assert(required == 0);
    return rv;
}


/* This function removes 'inaccessible' entries from the MPU map (those that are hidden by previous entries
 * in the map). It leaves any entries that match background entries in place.
 */
static void remove_inaccessible_entries(xthal_MPU_entry* fg, const xthal_MPU_entry* bg)
{
    int i;
    for (i = 1; i < XCHAL_MPU_ENTRIES; i++)
    {
        if (((XTHAL_MPU_ENTRY_GET_VALID(fg[i]) == XTHAL_MPU_ENTRY_GET_VALID(fg[i - 1])) && (XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]) > XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i - 1]))
                && (XTHAL_MPU_ENTRY_GET_MEMORY_TYPE(fg[i]) == XTHAL_MPU_ENTRY_GET_MEMORY_TYPE(fg[i - 1])) && (XTHAL_MPU_ENTRY_GET_ACCESS(fg[i]) == XTHAL_MPU_ENTRY_GET_ACCESS(fg[i - 1])) &&
                /* we can only remove the background map entry if either background alignment is not required, or
                 * if the previous entry is enabled.
                 */
                (!_xthal_in_bgmap(XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]), bg)))
                || ((!XTHAL_MPU_ENTRY_GET_VALID(fg[i]) && (!XTHAL_MPU_ENTRY_GET_VALID(fg[i - 1])) && (!_xthal_in_bgmap(XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]), bg)))))
        {
            write_map_entry(fg, i, fg[i - 1]);
        }
    }
}

/* This function takes bitwise or'd combination of access rights and memory type, and extracts
 * the access rights.  It returns the access rights, or -1.
 */
static int encode_access_rights(int cattr)
{
    cattr = cattr & 0xF;
    if ((cattr) > 0 && (cattr < 4))
        return -1;
    else
        return cattr;
}

/*
 * returns the largest value rv, such that for every index < rv,
 * entrys[index].vStartAddress < first.
 *
 * Assumes an ordered entry array (even disabled entries must be ordered).
 * value returned is in the range [0, XCHAL_MPU_ENTRIES].
 *
 */
static int find_entry(xthal_MPU_entry* fg, unsigned first)
{
    int i;
    for (i = XCHAL_MPU_ENTRIES - 1; i >= 0; i--)
    {
        if (XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]) <= first)
            return i + 1;
    }
    return 0; // if it is less than all existing entries return 0
}

/*
 * This function returns 1 if there is an exact match for first and first+size
 * so that no manipulations are necessary before safing and updating the attributes
 * for [first, first+size). The the first and end entries
 * must be valid, as well as all the entries in between.  Otherwise the memory
 * type might change across the region and we wouldn't be able to safe the caches.
 *
 * An alternative would be to require alignment regions in this case, but that seems
 * more wasteful.
 */
static int needed_entries_exist(xthal_MPU_entry* fg, unsigned first, unsigned last)
{
    int i;
    for (i = 0; i < XCHAL_MPU_ENTRIES; i++)
    {
        if (XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]) == first)
        {
            int j;
            /* special case ... is last at the end of the address space
             * ... if so there is no end entry needed.
             */
            if (last == 0xFFFFFFFF)
            {
                int k;
                for (k = i; k < XCHAL_MPU_ENTRIES; k++)
                    if (!XTHAL_MPU_ENTRY_GET_VALID(fg[k]))
                        return 0;
                return 1;
            }
            /* otherwise search for the end entry */
            for (j = i; j < XCHAL_MPU_ENTRIES; j++)
                if (last == XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[j]))
                {
                    int k;
                    for (k = i; k <= j; k++)
                        if (!XTHAL_MPU_ENTRY_GET_VALID(fg[k]))
                            return 0;
                    return 1;
                }
            return 0;
        }
    }
    return 0;
}

/* This function computes the number of MPU entries that are available for use in creating a new
 * region.
 */
static int number_available(xthal_MPU_entry* fg)
{
    int i;
    int rv = 0;
    int valid_seen = 0;
    for (i = 0; i < XCHAL_MPU_ENTRIES; i++)
    {
        if (!valid_seen)
        {
            if (XTHAL_MPU_ENTRY_GET_VALID(fg[i]))
                valid_seen = 1;
            else
            {
                rv++;
                continue;
            }
        }
        else
        {
            if (i && (XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]) == XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i - 1])))
                rv++;
        }
    }
    return rv;
}

/*
 * This function returns index of the background map entry that maps the address 'first' if there are no
 * enabled/applicable foreground map entries.
 */
static int get_bg_map_index(const xthal_MPU_entry* bg, unsigned first)
{
    int i;
    for (i = XCHAL_MPU_BACKGROUND_ENTRIES - 1; i >= 0; i--)
        if (first > XTHAL_MPU_ENTRY_GET_VSTARTADDR(bg[i]))
            return i;
    return 0;
}

inline static unsigned int covert_to_writethru_memtype(unsigned int wb_memtype)
{
    unsigned int prefix = wb_memtype & 0x1f0;
    if (prefix == 0x10)
      return wb_memtype & 0xfffffffe;
    else
      return wb_memtype & 0xffffffee;
}

/*
 * This function takes the region pointed to by ip, and makes it safe from the aspect of cache coherency, before
 * changing the memory type and possibly corrupting the cache. If wb is 0, then that indicates
 * that we should ignore uncommitted entries. If the inv argument is 0 that indicates that we shouldn't invalidate
 * the cache before switching to bypass.
 */
static void safe_region(xthal_MPU_entry* fg, int ip, unsigned end_of_segment, int memoryType, int wb, int inv,
        unsigned int* post_inv_all)
{
    unsigned length = end_of_segment - XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[ip]); // initially keep length 27msb to avoid possibility of overflow
    if (!length)
        return; // if the region is empty, there is no need to safe it

    int cmemType = XTHAL_MPU_ENTRY_GET_MEMORY_TYPE(fg[ip]);

    if (memoryType == cmemType)
        return; // not changing memory types ... we don't need to do anything

    int mt_is_wb = is_writeback(memoryType);
    int mt_is_ch = is_cacheable(memoryType);

    // nothing needs to be done in these cases
    if (mt_is_wb || (!wb && (!inv || mt_is_ch)))
        return;

    int need_flush = wb && (is_writeback(cmemType) && !is_writeback(memoryType));
    int need_invalidate = inv && (is_cacheable(cmemType) && !is_cacheable(memoryType));

    void* addr = (void*) XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[ip]);

    int write_by_region = length < CACHE_REGION_THRESHOLD;

    if (need_flush)
    {
        XTHAL_MPU_ENTRY_SET_MEMORY_TYPE(fg[ip], covert_to_writethru_memtype(XTHAL_MPU_ENTRY_GET_MEMORY_TYPE(fg[ip])));
        // If the AR == NONE, the writing back the cache may generate exception.  Temporarily open up the protections ...
        // ...
        if (XTHAL_MPU_ENTRY_GET_ACCESS(fg[ip]) == XTHAL_AR_NONE)
        	XTHAL_MPU_ENTRY_SET_ACCESS(fg[ip], XTHAL_AR_RWXrwx);
        // bit 0 determines if it wb/wt
        write_map_entry(fg, ip, fg[ip]);
            if (!write_by_region)
            {
                /* unfortunately there is no straight forward way to avoid the possibility of doing
                 * multiple xthal_dcache_all_writeback() calls during a region update.  The reason for this
                 * is that:
                 *
                 * 1) The writeback must be done before the memory type is changed to non-cacheable before
                 * an invalidate (see below)
                 *
                 * 2) it isn't possible to reorganize the loop so that all the writebacks are done before
                 * any of the invalidates because if part of the region of interest is (initially) mapped
                 * by the background map, then a single foreground entry is reused to 'safe' across
                 * each background map entry that is overlapped.
                 */
                xthal_dcache_all_writeback();
            }
            else if (length)
                xthal_dcache_region_writeback(addr, length);
    }

    if (need_invalidate)
    {
        XTHAL_MPU_ENTRY_SET_MEMORY_TYPE(fg[ip],
                               XTHAL_ENCODE_MEMORY_TYPE(XCHAL_CA_BYPASS));
        write_map_entry(fg, ip, fg[ip]);
        /* only need to call all_invalidate once ... check
         * if it has already been done.
         */
        if (!*post_inv_all)
        {
            if (!write_by_region)
            {
                *post_inv_all = 1;
            }
            else if (length)
            {
                xthal_icache_region_invalidate(addr, length);
                xthal_dcache_region_writeback_inv(addr, length);
            }
        }
    }
}

static unsigned max(unsigned a, unsigned b, unsigned c)
{
    if (a > b && a > c)
        return a;
    else if (b > c)
        return b;
    else
        return c;
}

/* This function returns the next address to commit which will be the greatest of the following:
 *      1) The start of the region we are creating
 *      2) The vStartAddress of the previous entry
 *      3) The background map entry that precedes the current address (last address committed).
 */
static unsigned next_address_to_commit(xthal_MPU_entry* fg, const xthal_MPU_entry* bg, unsigned first,
        int current_index)
{
    unsigned current = XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[current_index]);
    return max(first, XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[current_index - 1]), XTHAL_MPU_ENTRY_GET_VSTARTADDR(bg[get_bg_map_index(bg, current)]));
}

/*
 * This function does a series of calls to safe_region() to ensure that no data will be corrupted when changing the memory type
 * of an MPU entry. These calls are made for every entry address in the range[first,end), as well as at any background region boundary
 * in the range[first,end).  In general it is necessary to safe at the background region boundaries, because the memory type could
 * change at that address.
 *
 * This function is written to reuse already needed entries for the background map 'safes' which complicates things somewhat.
 *
 * After the calls to safe region are complete, then the entry attributes are updated for every entry in the range [first,end).
 */
static void safe_and_commit_overlaped_regions(xthal_MPU_entry* fg, const xthal_MPU_entry*bg, unsigned first,
        unsigned last, int memoryType, int accessRights, int wb, int inv)
{
    int i;
    unsigned int next;
    unsigned end_of_segment = last;
    unsigned post_inv_all = 0;
    unsigned int cachedisadr;
    write_cacheadrdis(0);
    for (i = XCHAL_MPU_ENTRIES - 1; i >= 0; i--)
        if (XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]) < last)
        {
            // first we want to commit the first entry
            safe_region(fg, i, end_of_segment, memoryType, wb, inv, &post_inv_all);
            end_of_segment = XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]);
            do
            {
                next = next_address_to_commit(fg, bg, first, i);
                if (next == XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i - 1]))
                    i--;
                XTHAL_MPU_ENTRY_SET_VSTARTADDR(fg[i], next);
                safe_region(fg, i, last, memoryType, wb, inv, &post_inv_all);
                end_of_segment = XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]);
            } while (next > first);
            if (post_inv_all)
            {
                xthal_icache_all_invalidate();
                xthal_dcache_all_writeback_inv();
            }
            for (; i < XCHAL_MPU_ENTRIES && XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]) < last; i++)
            {
                XTHAL_MPU_ENTRY_SET_MEMORY_TYPE(fg[i], memoryType);
                XTHAL_MPU_ENTRY_SET_ACCESS(fg[i], accessRights);
                XTHAL_MPU_ENTRY_SET_VALID(fg[i], 1);
                write_map_entry(fg, i, fg[i]);
            }
            break;
        }
    cachedisadr = xthal_calc_cacheadrdis(fg, XCHAL_MPU_ENTRIES);
    write_cacheadrdis(cachedisadr);
}

static void handle_invalid_pred(xthal_MPU_entry* fg, const xthal_MPU_entry* bg, unsigned first, int ip)
{
    /* Handle the case where there is an invalid entry immediately preceding the entry we
     * are creating.  If the entries addresses correspond to the same bg map, then we
     * make the previous entry valid with same attributes as the background map entry.
     *
     * The case where an invalid entry exists immediately preceding whose address corresponds to a different
     * background map entry is handled by create_aligning_entries_if_required(), so nothing is done here.
     */
    /* todo ... optimization opportunity,  the following block loops through the background map up to 4 times,
     *
     */
    if (!ip || XTHAL_MPU_ENTRY_GET_VALID(fg[ip - 1]))
        return;
    {
        int i;
        unsigned fgipm1_addr = XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[ip - 1]);
        int first_in_bg_map = 0;
        int first_bg_map_index = -1;
        int fgipm1_bg_map_index = -1;
#if MPU_DEVELOPMENT_MODE
        unsigned fgip_addr = XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[ip]);
        int fgip_bg_map_index = -1;
#endif
        for (i = XCHAL_MPU_BACKGROUND_ENTRIES - 1; i >= 0; i--)
        {
            unsigned addr = XTHAL_MPU_ENTRY_GET_VSTARTADDR(bg[i]);
            if (addr == first)
                first_in_bg_map = 1;
            if (addr < fgipm1_addr && fgipm1_bg_map_index == -1)
                fgipm1_bg_map_index = i;
#if MPU_DEVELOPMENT_MODE
            if (addr < fgip_addr && fgip_bg_map_index == -1)
                fgip_bg_map_index = i;
#endif
            if (addr < first && first_bg_map_index == -1)
                first_bg_map_index = i;
        }
        if (!first_in_bg_map && (first_bg_map_index == fgipm1_bg_map_index))
        {
            // There should be a subsequent entry that falls in the address range of same
            // background map entry ... if not, we have a problem because the following
            // will corrupt the memory map
#if MPU_DEVELOPMENT_MODE
            {
                my_assert(fgip_bg_map_index == fgipm1_bg_map_index);
            }
#endif
            xthal_MPU_entry temp = _xthal_get_entry(fg, bg, XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[ip - 1]), 0);
            XTHAL_MPU_ENTRY_SET_VSTARTADDR(temp, XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[ip - 1]));
            write_map_entry(fg, ip - 1, temp);
        }
    }
}

/* This function inserts a entry (unless it already exists) with vStartAddress of first.  The new entry has
 * the same accessRights and memoryType as the address first had before the call.
 *
 * If 'invalid' is specified, then insert an invalid region if no foreground entry exists for the address 'first'.
 */
static int insert_entry_if_needed_with_existing_attr(xthal_MPU_entry* fg, const xthal_MPU_entry* bg,
        unsigned first, int invalid)
{
    int i;
    int ip;
    int infg;
    int found = 0;

    for (i = XCHAL_MPU_ENTRIES - 1; i >= 0; i--)
        if (XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]) == first)
        {
            if (XTHAL_MPU_ENTRY_GET_VALID(fg[i]) || invalid)
                return XTHAL_SUCCESS;
            else
            {
                found = 1;
                ip = i;
                break;
            }
        }

    if (!found)
    {
        if (!number_available(fg))
            return XTHAL_OUT_OF_ENTRIES;

        ip = find_entry(fg, first);
        ip = bubble_free_to_ip(fg, ip, 1);
    }
    if (!invalid)
        handle_invalid_pred(fg, bg, first, ip);
    xthal_MPU_entry n;
    memset(&n, 0, sizeof(n));
    n = _xthal_get_entry(fg, bg, first, &infg);

    if (invalid && !infg) // If the entry mapping is currently in the foreground we can't make
        // the entry invalid without corrupting the attributes of the following entry.
        XTHAL_MPU_ENTRY_SET_VALID(n, 0);
    XTHAL_MPU_ENTRY_SET_VSTARTADDR(n,first);
    write_map_entry(fg, ip, n);
    return XTHAL_SUCCESS;
}

static unsigned int smallest_entry_greater_than_equal(xthal_MPU_entry* fg, unsigned x)
{
    int i;
    for (i = 0; i < XCHAL_MPU_ENTRIES; i++)
        if (XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]) >= x)
            return XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]);
    return 0;
}

/* This function creates background map aligning entries if required.*/
static unsigned int create_aligning_entries_if_required(xthal_MPU_entry* fg, const xthal_MPU_entry* bg,
        unsigned x)
{
#if XCHAL_MPU_ALIGN_REQ
    int i;
    int rv;
    unsigned next_entry_address = 0;
    unsigned next_entry_valid = 0;
    int preceding_bg_entry_index_x = get_bg_map_index(bg, x);
    unsigned preceding_bg_entry_x_addr = XTHAL_MPU_ENTRY_GET_VSTARTADDR(bg[preceding_bg_entry_index_x]);
    for (i = XCHAL_MPU_ENTRIES - 1; i >= 0; i--)
    {
        if (XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]) < x)
        {
            if (XTHAL_MPU_ENTRY_GET_VALID(fg[i]))
                return XTHAL_SUCCESS; // If there is a valid entry immediately before the proposed new entry
            // ... then no aligning entries are required
            break;
        }
        else
        {
            next_entry_address = XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]);
            next_entry_valid = XTHAL_MPU_ENTRY_GET_VALID(fg[i]);
        }
    }

    /*
     * before creating the aligning entry, we may need to create an entry or entries a higher
     * addresses to limit the scope of the aligning entry.
     */
    if ((!next_entry_address) || (!next_entry_valid) || (_xthal_in_bgmap(next_entry_address, bg)))
    {
        /* in this case, we can just create an invalid entry at the start of the new region because
         * a valid entry could have an alignment problem.  An invalid entry is safe because we know that
         * the next entry is either invalid, or is on a bg map entry
         */
        if ((rv = insert_entry_if_needed_with_existing_attr(fg, bg, x, 1)) != XTHAL_SUCCESS)
        {
            return rv;
        }
    }
    else
    {
        unsigned next_bg_entry_index;
        for (next_bg_entry_index = 0; next_bg_entry_index < XCHAL_MPU_BACKGROUND_ENTRIES; next_bg_entry_index++)
            if (XTHAL_MPU_ENTRY_GET_VSTARTADDR(bg[next_bg_entry_index]) > x)
                break;
        if (next_entry_address == XTHAL_MPU_ENTRY_GET_VSTARTADDR(bg[next_bg_entry_index])) // In this case there is no intervening bg entry
        // between the new entry x, and the next existing entry so, we don't need any limiting entry
        // (the existing next_entry serves as the limiting entry)
        { /* intentionally empty */
        }
        else
        {
            // In this case we need to create a valid region at the background entry that immediately precedes
            // next_entry_address, and then create an invalid entry at the background entry immediately after
            // x
            if ((rv = insert_entry_if_needed_with_existing_attr(fg, bg, XTHAL_MPU_ENTRY_GET_VSTARTADDR(_xthal_get_entry(fg, bg, x, 0)), 0))
                    != XTHAL_SUCCESS)
            {
                return rv;
            }
            if ((rv = insert_entry_if_needed_with_existing_attr(fg, bg,
                    XTHAL_MPU_ENTRY_GET_VSTARTADDR(_xthal_get_entry(fg, bg, XTHAL_MPU_ENTRY_GET_VSTARTADDR(bg[next_bg_entry_index]), 0)), 1)) != XTHAL_SUCCESS)
            {
                return rv;
            }
        }
    }

    /* now we are finally ready to create the aligning entry.*/
    if (!(x == preceding_bg_entry_x_addr))
        if ((rv = insert_entry_if_needed_with_existing_attr(fg, bg, preceding_bg_entry_x_addr, 0)) != XTHAL_SUCCESS)
        {
            return rv;
        }

    return XTHAL_SUCCESS;

#else
    return XTHAL_SUCCESS;
#endif
}

static unsigned start_initial_region(xthal_MPU_entry* fg, const xthal_MPU_entry* bg, unsigned first,
        unsigned end)
{
    int i;
    unsigned addr;
    for (i = XCHAL_MPU_BACKGROUND_ENTRIES - 1; i >= 0; i--)
    {
        addr = XTHAL_MPU_ENTRY_GET_VSTARTADDR(bg[i]);
        if (addr <= first)
            break;
        if (addr < end)
            return addr;
    }
    return first;
}

static int safe_add_region(unsigned first, unsigned last, unsigned accessRights, unsigned memoryType,
        unsigned writeback, unsigned invalidate)
{
    /* This function sets the memoryType and accessRights on a region of memory. If necessary additional MPU entries
     * are created so that the attributes of any memory outside the specified region are not changed.
     *
     * This function has 2 stages:
     * 	1) The map is updated one entry at a time to create (if necessary) new entries to mark the beginning and end of the
     * 	   region as well as addition alignment entries if needed.  During this stage the map is always correct, and the memoryType
     * 	   and accessRights for every address remain the same.
     * 	2) The entries inside the update region are then safed for cache consistency (if necessary) and then written with
     * 	   the new accessRights, and memoryType.
     *
     * If the function fails (by running out of available map entries) during stage 1 then everything is still consistent and
     * it is safe to return an error code.
     *
     * If XCHAL_MPU_ALIGN_REQ is provided then extra entries are create if needed
     * to satisfy these alignment conditions:
     *
     * 1) If entry0's Virtual Address Start field is nonzero, then that field must equal one of the Background Map's
     *    Virtual Address Start field values if software ever intends to assert entry0's MPUENB bit.
     * 2) If entryN's MPUENB bit will ever be negated while at the same time entryN+1's MPUENB bit is
     *    asserted, then entryN+1's Virtual Address Start field must equal one of the Background Map's Virtual Address Start field values.
     *
     * Between 0 and 2 available entries will be used by this function. In addition, if XCHAL_MPU_ALIGN_REQ == 1 up to ???
     * additional entries will be needed to meet background map alignment requirements.
     *
     * This function keeps a copy of the current map in 'fg'.  This is kept in sync with contents of the MPU at all times.
     *
     */

    int rv;

    xthal_MPU_entry fg[XCHAL_MPU_ENTRIES];
#if MPU_DEVELOPMENT_MODE
    xthal_MPU_entry on_entry[XCHAL_MPU_ENTRIES];
    xthal_read_map(on_entry);
#endif
    xthal_read_map(fg);
    assert_map_valid();

    /* First we check and see if consecutive entries at first, and first + size already exist.
     * in this important special case we don't need to do anything but safe and update the entries [first, first+size).
     *
     */

    if (!needed_entries_exist(fg, first, last))
    {
        unsigned x;
        unsigned pbg;

        /*
         * If we are tight on entries, the first step is to remove any redundant entries in the MPU
         * to make room to ensure that there is room for the new entries we need.
         *
         * We need to call it here ... once we have started transforming the map it is too late
         * (the process involves creating inaccessible entries that could potentially get removed).
         */
        if (number_available(fg) < XCHAL_MPU_WORST_CASE_ENTRIES_FOR_REGION)
            remove_inaccessible_entries(fg, Xthal_mpu_bgmap);
#if MPU_DEVELOPMENT_MODE
        assert_maps_equivalent(on_entry, fg, Xthal_mpu_bgmap);
#endif
        // First we create foreground entries that 'duplicate' background entries to aide in
        // maintaining proper alignment.
        if ((rv = create_aligning_entries_if_required(fg, Xthal_mpu_bgmap, first)) != XTHAL_SUCCESS)
            return rv;

        // First we write the terminating entry for our region
        // 5 cases:
        // 1) end is at the end of the address space, then we don't need to do anything ... takes 0 entries
        // 2) There is an existing entry at end ... another nop ... 0 entries
        // 3) end > than any existing entry ... in this case we just create a new invalid entry at end to mark
        //    end of the region.  No problem with alignment ... this takes 1 entry
        // 4) otherwise if there is a background map boundary between end and x ,the smallest existing entry that is
        //    greater than end, then we first create an equivalent foreground map entry for the background map entry that immediately
        //    precedes x, and then we write an invalid entry for end. Takes 2 entries
        // 5) otherwise x is in the same background map entry as end, in this case we write a new foreground entry with the existing
        //    attributes at end

        if (last == 0xFFFFFFFF)
        { /* the end is the end of the address space ... do nothing */
        }
        else
        {
            x = smallest_entry_greater_than_equal(fg, last);
            if (last == x)
            { /* another nop */
            }
            else if (last > x)
            { /* there is no entry that has a start after the new region ends
             ... we handle this by creating an invalid entry at the end point */
                if ((rv = insert_entry_if_needed_with_existing_attr(fg, Xthal_mpu_bgmap, last, 1)) != XTHAL_SUCCESS)
                {
#if MPU_DEVELOPMENT_MODE
                    assert_maps_equivalent(on_entry, fg, Xthal_mpu_bgmap);
#endif
                    return rv;
                }
#if MPU_DEVELOPMENT_MODE
                assert_maps_equivalent(on_entry, fg, Xthal_mpu_bgmap);
#endif
            }
            else
            {
                pbg = XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[get_bg_map_index(Xthal_mpu_bgmap, x)]);
                /* so there is an existing entry we must deal with.  We next need to find
                 * if there is an existing background entry in between the end of
                 * the new region and beginning of the next.
                 */
                if ((pbg != x) && (pbg > last))
                {
                    /* okay ... there is an intervening background map entry.  We need
                     * to handle this by inserting an aligning entry (if the architecture requires it)
                     * and then placing writing an invalid entry at end.
                     */
                    if (XCHAL_MPU_ALIGN_REQ)
                    {
                        if ((rv = insert_entry_if_needed_with_existing_attr(fg, Xthal_mpu_bgmap, pbg, 0)) != XTHAL_SUCCESS)
                        {
#if MPU_DEVELOPMENT_MODE
                            assert_maps_equivalent(on_entry, fg, Xthal_mpu_bgmap);
#endif
                            return rv;
                        }
#if MPU_DEVELOPMENT_MODE
                        assert_maps_equivalent(on_entry, fg, Xthal_mpu_bgmap);
#endif
                    }
                    if ((rv = insert_entry_if_needed_with_existing_attr(fg, Xthal_mpu_bgmap, last, 1)) != XTHAL_SUCCESS)
                    {
#if MPU_DEVELOPMENT_MODE
                        assert_maps_equivalent(on_entry, fg, Xthal_mpu_bgmap);
#endif
                        return rv;
                    }
#if MPU_DEVELOPMENT_MODE
                    assert_maps_equivalent(on_entry, fg, Xthal_mpu_bgmap);
#endif
                }
                else
                /* ok so there are no background map entry in between end and x, in this case
                 * we just need to create a new entry at end writing the existing attributes.
                 */
                if ((rv = insert_entry_if_needed_with_existing_attr(fg, Xthal_mpu_bgmap, last, 1)) != XTHAL_SUCCESS)
                {
#if MPU_DEVELOPMENT_MODE
                    assert_maps_equivalent(on_entry, fg, Xthal_mpu_bgmap);
#endif
                    return rv;
                }
#if MPU_DEVELOPMENT_MODE
                assert_maps_equivalent(on_entry, fg, Xthal_mpu_bgmap);
#endif
            }
        }

        /* last, but not least we need to insert a entry at the starting address for our new region */
        if ((rv = insert_entry_if_needed_with_existing_attr(fg, Xthal_mpu_bgmap, start_initial_region(fg, Xthal_mpu_bgmap, first, last), 0))
                != XTHAL_SUCCESS)
        {
#if MPU_DEVELOPMENT_MODE
            assert_maps_equivalent(on_entry, fg, Xthal_mpu_bgmap);
#endif
            return rv;
        }
#if MPU_DEVELOPMENT_MODE
        assert_maps_equivalent(on_entry, fg, Xthal_mpu_bgmap);
#endif
    }
    // up to this point, the attributes of every byte in the address space should be the same as when this function
    // was called.
    safe_and_commit_overlaped_regions(fg, Xthal_mpu_bgmap, first, last, memoryType, accessRights, writeback, invalidate);

    assert_map_valid();
    return XTHAL_SUCCESS;
}

// checks if x (full 32bit) is mpu_aligned for MPU
static unsigned int mpu_aligned(unsigned x)
{
    return !(x & MPU_ALIGNMENT_MASK);
}

static unsigned int mpu_align(unsigned int x, unsigned int roundUp)
{
    if (roundUp)
        return (x + MPU_ALIGNMENT_MASK) & MPU_ADDRESS_MASK;
    else
        return (x & MPU_ADDRESS_MASK);
}

#endif

#if defined(__SPLIT__mpu_check)
static int bad_accessRights(unsigned ar)
{
    if (ar == 0 || (ar >= 4 && ar <= 15))
        return 0;
    else
        return 1;
}

/* this function checks if the supplied map 'fg' is a valid MPU map using 3 criteria:
 * 		1) if an entry is valid, then that entries accessRights must be defined (0 or 4-15).
 * 		2) The map entries' 'vStartAddress's must be in increasing order.
 * 		3) If the architecture requires background map alignment then:
 * 			a) If entry0's 'vStartAddress' field is nonzero, then that field must equal
 * 			one of the Background Map's 'vStartAddress' field values if the entry 0's valid bit is set.
 * 			b) If entryN's 'valid' bit is 0 and entry[N+1]'s 'valid' bit is 1, then
 * 			 entry[N+1]'s 'vStartAddress' field must equal one of the Background Map's 'vStartAddress' field values.
 *
 *		This function returns XTHAL_SUCCESS if the map satisfies the condition, otherwise it returns
 *		XTHAL_BAD_ACCESS_RIGHTS, XTHAL_OUT_OF_ORDER_MAP, or XTHAL_MAP_NOT_ALIGNED.
 *
 */
static int check_map(const xthal_MPU_entry* fg, unsigned int n, const xthal_MPU_entry* bg)
{
    int i;
    unsigned current = 0;
    if (!n)
        return XTHAL_SUCCESS;
    if (n > XCHAL_MPU_ENTRIES)
        return XTHAL_OUT_OF_ENTRIES;
    for (i = 0; i < n; i++)
    {
        if (XTHAL_MPU_ENTRY_GET_VALID(fg[i]) && bad_accessRights(XTHAL_MPU_ENTRY_GET_ACCESS(fg[i])))
            return XTHAL_BAD_ACCESS_RIGHTS;
        if ((XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]) < current))
            return XTHAL_OUT_OF_ORDER_MAP;
        if (XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]) & MPU_VSTART_CORRECTNESS_MASK)
             return XTHAL_MAP_NOT_ALIGNED;
        current = XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i]);
    }
    if (XCHAL_MPU_ALIGN_REQ && XTHAL_MPU_ENTRY_GET_VALID(fg[0]) && XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[0])
            && !_xthal_in_bgmap(XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[0]), bg))
        return XTHAL_MAP_NOT_ALIGNED;
    for (i = 0; i < n- 1; i++)
        if (XCHAL_MPU_ALIGN_REQ && !XTHAL_MPU_ENTRY_GET_VALID(fg[i]) && XTHAL_MPU_ENTRY_GET_VALID(fg[i + 1])
                && !_xthal_in_bgmap(XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[i + 1]), bg))
            return XTHAL_MAP_NOT_ALIGNED;
    return XTHAL_SUCCESS;
}



/*
 * this function checks that the bit-wise or-ed XTHAL_MEM_... bits in x correspond to a valid
 * MPU memoryType. If x is valid, then 0 is returned, otherwise XTHAL_BAD_MEMORY_TYPE is
 * returned.
 */
static int check_memory_type(unsigned x)
{
    unsigned system_cache_type = _XTHAL_MEM_CACHE_MASK(x);
    unsigned processor_cache_type = (((x) & _XTHAL_LOCAL_CACHE_BITS) >> 4);
    if ((system_cache_type > XTHAL_MEM_NON_CACHEABLE) || (processor_cache_type > XTHAL_MEM_NON_CACHEABLE))
        return XTHAL_BAD_MEMORY_TYPE;
    int processor_cache_type_set = 1;
    if (!processor_cache_type)
    {
        processor_cache_type = system_cache_type << 4;
        processor_cache_type_set = 0;
    }
    unsigned device = _XTHAL_MEM_IS_DEVICE(x);
    unsigned system_noncacheable = _XTHAL_IS_SYSTEM_NONCACHEABLE(x);

    if (device | system_noncacheable)
    {
        if ((system_cache_type || processor_cache_type_set) && device)
            return XTHAL_BAD_MEMORY_TYPE;
        if (processor_cache_type_set)
            return XTHAL_BAD_MEMORY_TYPE; // if memory is device or non cacheable, then processor cache type should not be set
        if (system_noncacheable && (x & XTHAL_MEM_INTERRUPTIBLE))
            return XTHAL_BAD_MEMORY_TYPE;
        {
            unsigned z = x & XTHAL_MEM_SYSTEM_SHAREABLE;
            if ((z == XTHAL_MEM_INNER_SHAREABLE) || (z == XTHAL_MEM_OUTER_SHAREABLE))
                return XTHAL_BAD_MEMORY_TYPE;
        }
    }
    else
    {
        if ((x & XTHAL_MEM_SYSTEM_SHAREABLE) == XTHAL_MEM_SYSTEM_SHAREABLE)
            return XTHAL_BAD_MEMORY_TYPE;
        if ((x & (XTHAL_MEM_BUFFERABLE | XTHAL_MEM_INTERRUPTIBLE)))
            return XTHAL_BAD_MEMORY_TYPE;
    }

    return 0;
}
#endif

#endif // is MPU

#if defined(__SPLIT__mpu_basic)
/*
 * These functions accept encoded access rights, and return 1 if the supplied memory type has the property specified by the function name.
 */
extern int xthal_is_kernel_readable(int accessRights)
{
#if XCHAL_HAVE_MPU
    return is_kernel_readable(accessRights);
#else
    return XTHAL_UNSUPPORTED;
#endif
}

extern int xthal_is_kernel_writeable(int accessRights)
{
#if XCHAL_HAVE_MPU
    return is_kernel_writeable(accessRights);
#else
    return XTHAL_UNSUPPORTED;
#endif
}

extern int xthal_is_kernel_executable(int accessRights)
{
#if XCHAL_HAVE_MPU
    return is_kernel_executable(accessRights);
#else
    return XTHAL_UNSUPPORTED;
#endif
}

extern int xthal_is_user_readable(int accessRights)
{
#if XCHAL_HAVE_MPU
    return is_user_readable(accessRights);
#else
    return XTHAL_UNSUPPORTED;
#endif
}

extern int xthal_is_user_writeable(int accessRights)
{
#if XCHAL_HAVE_MPU
    return is_user_writeable(accessRights);
#else
    return XTHAL_UNSUPPORTED;
#endif
}

extern int xthal_is_user_executable(int accessRights)
{
#if XCHAL_HAVE_MPU
    return is_user_executable(accessRights);
#else
    return XTHAL_UNSUPPORTED;
#endif
}

/*
 * These functions accept either an encoded or unencoded memory type, and
 * return 1 if the supplied memory type has property specified by the
 * function name.
 */
int xthal_is_cacheable(unsigned int mt)
{
#if XCHAL_HAVE_MPU
    return is_cacheable(mt);
#else
    return XTHAL_UNSUPPORTED;
#endif
}

int xthal_is_writeback(unsigned int mt)
{
#if XCHAL_HAVE_MPU
    return is_writeback(mt);
#else
    return XTHAL_UNSUPPORTED;
#endif
}

int xthal_is_device(unsigned int mt)
{
#if XCHAL_HAVE_MPU
    return is_device(mt);
#else
    return XTHAL_UNSUPPORTED;
#endif
}
#endif

/*
 * This function converts a bit-wise combination of the XTHAL_MEM_.. constants
 * to the corresponding MPU memory type (9-bits).
 *
 * If none of the XTHAL_MEM_.. bits are present in the argument, then
 * bits 4-12 (9-bits) are returned ... this supports using an already encoded
 * memoryType (perhaps obtained from an xthal_MPU_entry structure) as input
 * to xthal_set_region_attribute().
 *
 * This function first checks that the supplied constants are a valid and
 * supported combination.  If not, it returns XTHAL_BAD_MEMORY_TYPE.
 */
#if defined(__SPLIT__mpu_check)
int xthal_encode_memory_type(unsigned int x)
{
#if XCHAL_HAVE_MPU
    const unsigned int MemoryTypeMask = 0x1ff0;
    const unsigned int MemoryFlagMask = 0xffffe000;
    /*
     * Encodes the memory type bits supplied in an | format (XCHAL_CA_PROCESSOR_CACHE_WRITEALLOC | XCHAL_CA_PROCESSOR_CACHE_WRITEBACK)
     */
    unsigned memoryFlags = x & MemoryFlagMask;
    if (!memoryFlags)
        return (x & MemoryTypeMask) >> XTHAL_AR_WIDTH;
    else
    {
        int chk = check_memory_type(memoryFlags);
        if (chk < 0)
            return chk;
        else
            return XTHAL_ENCODE_MEMORY_TYPE(memoryFlags);
    }
#else
    return XTHAL_UNSUPPORTED;
#endif
}
#endif

#if defined(__SPLIT__mpu_rmap)

/*
 * Copies the current MPU entry list into 'entries' which
 * must point to available memory of at least
 * sizeof(xthal_MPU_entry) * XCHAL_MPU_ENTRIES.
 *
 * This function returns XTHAL_SUCCESS.
 * XTHAL_INVALID, or
 * XTHAL_UNSUPPORTED.
 */
int xthal_read_map(xthal_MPU_entry* fg_map)
{
#if XCHAL_HAVE_MPU
    unsigned i;
    if (!fg_map)
        return XTHAL_INVALID;
    xthal_read_map_raw(fg_map);
    return XTHAL_SUCCESS;
#else
    return XTHAL_UNSUPPORTED;
#endif
}

#if XCHAL_HAVE_MPU
 #undef XCHAL_MPU_BGMAP
 #define XCHAL_MPU_BGMAP(s,vstart,vend,rights,mtype,x...) XTHAL_MPU_ENTRY(vstart,1,rights,mtype),
const xthal_MPU_entry Xthal_mpu_bgmap[] = { XCHAL_MPU_BACKGROUND_MAP(0) };
#endif


/*
 * Copies the MPU background map into 'entries' which must point
 * to available memory of at least
 * sizeof(xthal_MPU_entry) * XCHAL_MPU_BACKGROUND_ENTRIES.
 *
 * This function returns XTHAL_SUCCESS.
 * XTHAL_INVALID, or
 * XTHAL_UNSUPPORTED.
 */
int xthal_read_background_map(xthal_MPU_entry* bg_map)
{
#if XCHAL_HAVE_MPU
    if (!bg_map)
        return XTHAL_INVALID;
    memcpy(bg_map, Xthal_mpu_bgmap, sizeof(Xthal_mpu_bgmap));
    return XTHAL_SUCCESS;
#else
    return XTHAL_UNSUPPORTED;
#endif
}
#endif
/*
 * Writes the map pointed to by 'entries' to the MPU. Before updating
 * the map, it commits any uncommitted
 * cache writes, and invalidates the cache if necessary.
 *
 * This function does not check for the correctness of the map.  Generally
 * xthal_check_map() should be called first to check the map.
 *
 * If n == 0 then the existing map is cleared, and no new map is written
 * (useful for returning to reset state)
 *
 * If (n > 0 && n < XCHAL_MPU_ENTRIES) then a new map is written with
 * (XCHAL_MPU_ENTRIES-n) padding entries added to ensure a properly ordered
 * map.  The resulting foreground map will be equivalent to the map vector
 * fg, but the position of the padding entries should not be relied upon.
 *
 * If n == XCHAL_MPU_ENTRIES then the complete map as specified by fg is
 * written.
 *
 * xthal_write_map() disables the MPU foreground map during the MPU
 * update and relies on the background map.
 *
 * As a result any interrupt that does not meet the following conditions
 * must be disabled before calling xthal_write_map():
 *    1) All code and data needed for the interrupt must be
 *       mapped by the background map with sufficient access rights.
 *    2) The interrupt code must not access the MPU.
 *
 */
#if defined(__SPLIT__mpu_wmap)
void xthal_write_map(const xthal_MPU_entry* fg, unsigned int n)
{
#if XCHAL_HAVE_MPU
    unsigned int cacheadrdis = xthal_calc_cacheadrdis(fg, n);
    xthal_dcache_all_writeback_inv();
    xthal_icache_all_invalidate();
    xthal_write_map_raw(fg, n);
    write_cacheadrdis(cacheadrdis);
    isync(); // ditto
#endif
}
#endif

#if defined(__SPLIT__mpu_check)
/*
 * Checks if entry vector 'fg' of length 'n' is a valid MPU access map.
 * Returns:
 *    XTHAL_SUCCESS if valid,
 *    XTHAL_OUT_OF_ENTRIES
 *    XTHAL_MAP_NOT_ALIGNED,
 *    XTHAL_BAD_ACCESS_RIGHTS,
 *    XTHAL_OUT_OF_ORDER_MAP, or
 *    XTHAL_UNSUPPORTED if config doesn't have an MPU.
 */
int xthal_check_map(const xthal_MPU_entry* fg, unsigned int n)
{
#if XCHAL_HAVE_MPU
    return check_map(fg, XCHAL_MPU_ENTRIES, Xthal_mpu_bgmap);
#else
    return XTHAL_UNSUPPORTED;
#endif
}
#endif

#if defined(__SPLIT__mpu_basic)
/*
 * Returns the MPU entry that maps 'vaddr'. If 'infgmap' is non-NULL then it is
 * set to 1 if 'vaddr' is mapped by the foreground map, or 0 if 'vaddr'
 * is mapped by the background map.
 */
extern xthal_MPU_entry xthal_get_entry_for_address(void* paddr, int* infgmap)
  {
#if XCHAL_HAVE_MPU
    xthal_MPU_entry e;
    unsigned int p;
    __asm__ __volatile__("PPTLB %0, %1\n\t" : "=a" (p) : "a" (paddr));
    if ((p & 0x80000000))
      {
	if (infgmap)
	   *infgmap = 1;
	e.at = (p & 0x1fffff);
	__asm__ __volatile__("RPTLB0 %0, %1\n\t" : "=a" (e.as) : "a" (p & 0x1f));
	return e;
      }
    else
      {
	int i;
	if (infgmap)
	*infgmap = 0;
	for (i = XCHAL_MPU_BACKGROUND_ENTRIES - 1; i > 0; i--)
	  {
	    if (XTHAL_MPU_ENTRY_GET_VSTARTADDR(Xthal_mpu_bgmap[i]) <= (unsigned) paddr)
	      {
		return Xthal_mpu_bgmap[i];
	      }
	  } // in background map
	return Xthal_mpu_bgmap[0];
      }
#else
    xthal_MPU_entry e;
    return e;
#endif
  }
#endif
/*
 * This function is intended as an MPU specific version of
 * xthal_set_region_attributes(). xthal_set_region_attributes() calls
 * this function for MPU configurations.
 *
 * This function sets the attributes for the region [vaddr, vaddr+size)
 * in the MPU.
 *
 * Depending on the state of the MPU this function will require from
 * 0 to 3 unused MPU entries.
 *
 * This function typically will move, add, and subtract entries from
 * the MPU map during execution, so that the resulting map may
 * be quite different than when the function was called.
 *
 * This function does make the following guarantees:
 *    1) The MPU access map remains in a valid state at all times
 *       during its execution.
 *    2) At all points during (and after) completion the memoryType
 *       and accessRights remain the same for all addresses
 *       that are not in the range [vaddr, vaddr+size).
 *    3) If XTHAL_SUCCESS is returned, then the range
 *       [vaddr, vaddr+size) will have the accessRights and memoryType
 *       specified.
 *
 * The accessRights parameter should be either a 4-bit value corresponding
 * to an MPU access mode (as defined by the XTHAL_AR_.. constants), or
 * XTHAL_MPU_USE_EXISTING_ACCESS_RIGHTS.
 *
 * The memoryType parameter should be either a bit-wise or-ing of XTHAL_MEM_..
 * constants that represent a valid MPU memoryType, a 9-bit MPU memoryType
 * value, or XTHAL_MPU_USE_EXISTING_MEMORY_TYPE.
 *
 * In addition to the error codes that xthal_set_region_attribute()
 * returns, this function can also return: XTHAL_BAD_ACCESS_RIGHTS
 * (if the access rights bits map to an unsupported combination), or
 * XTHAL_OUT_OF_ENTRIES (if there are not enough unused MPU entries).
 *
 * If this function is called with an invalid MPU map, then this function
 * will return one of the codes that is returned by xthal_check_map().
 *
 * The flag, XTHAL_CAFLAG_EXPAND, is not supported.
 *
 */
#if defined(__SPLIT__mpu_attributes)
int xthal_mpu_set_region_attribute(void* vaddr, unsigned size, int accessRights, int memoryType, unsigned flags)
{
#if XCHAL_HAVE_MPU
    unsigned int first;
    unsigned int last;
    int rv;

    if (flags & XTHAL_CAFLAG_EXPAND)
        return XTHAL_UNSUPPORTED;
    if (size == 0)
        return XTHAL_ZERO_SIZED_REGION;
    first = (unsigned) vaddr;
    last = first + size;
    if (last != 0xFFFFFFFF)
        last--;
    if (first >= last)
        return XTHAL_INVALID_ADDRESS_RANGE; // Wraps around

    if (accessRights & XTHAL_MPU_USE_EXISTING_ACCESS_RIGHTS)
    {
        accessRights = XTHAL_MPU_ENTRY_GET_ACCESS(xthal_get_entry_for_address(vaddr, 0));
    }
    else
    {
        accessRights = encode_access_rights(accessRights);
        if (accessRights < 0)
            return XTHAL_BAD_ACCESS_RIGHTS;
    }
    if (memoryType & XTHAL_MPU_USE_EXISTING_MEMORY_TYPE)
    {
        memoryType = XTHAL_MPU_ENTRY_GET_MEMORY_TYPE(xthal_get_entry_for_address(vaddr, 0));
    }
    else
    {
        if (memoryType & 0xffffe000) // Tests if any of the XTHAL MEM flags are present
           memoryType = xthal_encode_memory_type(memoryType);
        else
            if (memoryType & 0xfffffe00) // Tests if any of bits from 9 to 13 are set indicating
                // that the memoryType was improperly shifted
                // we flag this as an error
               return XTHAL_BAD_MEMORY_TYPE;
        if (memoryType < 0)
            return XTHAL_BAD_MEMORY_TYPE;
    }
    if (flags & XTHAL_CAFLAG_EXACT)
        if (!mpu_aligned(first) || !mpu_aligned(last + 1))
            return XTHAL_INEXACT;

    first = mpu_align(first, (flags & XTHAL_CAFLAG_NO_PARTIAL));
    if (last != 0xffffffff)
    {
        last = mpu_align(last + 1, !(flags & XTHAL_CAFLAG_NO_PARTIAL));
        if (first >= last)
            return ((flags & XTHAL_CAFLAG_NO_PARTIAL) ? XTHAL_ZERO_SIZED_REGION : 0);
    }
    rv = safe_add_region(first, last, accessRights, memoryType, !(flags & XTHAL_CAFLAG_NO_AUTO_WB),
            !(flags & XTHAL_CAFLAG_NO_AUTO_INV));
    isync();
    return rv;
#else
    return XTHAL_UNSUPPORTED;
#endif
}
#endif


#if defined(__SPLIT__mpu_cachedis)

inline static unsigned int max2(unsigned int a, unsigned int b)
  {
    if (a>b)
      return a;
    else
      return b;
  }

inline static unsigned int mask_cachedis(unsigned int current, int first_region,
    int last_region)
  {
    unsigned int x;
    x = ((1 << (last_region - first_region + 1)) - 1) << first_region;
    current &= ~x;
    return current;
  }

/*
 * xthal_calc_cacheadrdis() computes the value that should be written
 * to the CACHEADRDIS register.  The return value has bits 0-7 set according as:
 * bit n: is zero if any part of the region [512MB * n, 512MB* (n-1)) is cacheable.
 * 	     is one  if NO part of the region [512MB * n, 512MB* (n-1)) is cacheable.
 *
 * This function looks at both the loops through both the foreground and background maps
 * to find cacheable area.  Once one cacheable area is found in a 512MB region, then we
 * skip to the next 512MB region.
 */
unsigned int xthal_calc_cacheadrdis(const xthal_MPU_entry* fg, unsigned int num_entries)
  {
#if XCHAL_HAVE_MPU
    unsigned int cachedis = 0xff;
    int fg_index = num_entries - 1;
    int bg_index = XCHAL_MPU_BACKGROUND_ENTRIES - 1;
    int working_region = 7;
    int ending_region;
    unsigned int vaddr = 0xffffffff;
    while (bg_index >= 0 || fg_index >= 0)
      {
	if ((fg_index >= 0 && XTHAL_MPU_ENTRY_GET_VALID(fg[fg_index])))
	  {
	    vaddr = XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[fg_index]);
	    ending_region = vaddr >> 29;
	    if (ending_region <= working_region)
	      {
		unsigned int mt = XTHAL_MPU_ENTRY_GET_MEMORY_TYPE(fg[fg_index]);
		if (is_cacheable(mt))
		  {
		    cachedis = mask_cachedis(cachedis, ending_region,
			working_region);
		    /* Optimize since we have found one cacheable entry in the region ... no need to look for more */
		    if (ending_region == 0)
		      return cachedis;
		    else
		      working_region = ending_region - 1;
		  }
		else
		if (vaddr & 0x1fffffff) // If vaddr is on a 512MB region we want to move to the next region
		  working_region = ending_region;
		else
		  working_region = ending_region - 1;
	      }
	  }
	else if ((bg_index >= 0)
	    && ((fg_index <= 0)
		|| XTHAL_MPU_ENTRY_GET_VALID(fg[fg_index-1]))&& vaddr)
	  {
	    unsigned int caddr;
	    unsigned int low_addr = (
		(fg_index >= 0) ?
		(XTHAL_MPU_ENTRY_GET_VSTARTADDR(fg[fg_index])) :
		0);
	    /* First skip any background entries that start after the address of interest */
	    while ((caddr = XTHAL_MPU_ENTRY_GET_VSTARTADDR(Xthal_mpu_bgmap[bg_index])) >= vaddr)
	    bg_index--;
	    do
	      {
		caddr = max2(XTHAL_MPU_ENTRY_GET_VSTARTADDR(Xthal_mpu_bgmap[bg_index]),
		    low_addr);
		ending_region = caddr >> 29;
		if (ending_region <= working_region)
		  {
		    unsigned int mt = XTHAL_MPU_ENTRY_GET_MEMORY_TYPE(
			Xthal_mpu_bgmap[bg_index]);
		    if (is_cacheable(mt))
		      {
			cachedis = mask_cachedis(cachedis, ending_region,
			    working_region);
			/* Optimize since we have found one cacheable entry in the region ...
			 * no need to look for more */
			if (ending_region == 0)
			  return cachedis; // we are done
			else
			  working_region = ending_region - 1;
		      }
		    else
		    if (caddr & 0x1fffffff)
		      working_region = ending_region;
		    else
		      working_region = ending_region - 1;
		  }
		bg_index--;
	      }while (caddr > low_addr);
	    vaddr = caddr;
	  }
	fg_index--;
	if (!vaddr)
	break;
      }
    return cachedis;
#else
    return 0;
#endif
  }
#endif

#if defined(__SPLIT__mpu_basic)
void (*_xthal_assert_handler)();
/* Undocumented internal testing function */
extern void _xthal_set_assert_handler(void (*handler)())
{
#if XCHAL_HAVE_MPU
    _xthal_assert_handler = handler;
#endif
}
#endif
