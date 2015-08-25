lib_LIBRARIES = libhal.a

libhal_a_SOURCES = \
	attribute.c \
	cache.c \
	clock.S \
	coherence.c \
	debug.c \
	debug_hndlr.S \
	disass.c \
	memcopy.S \
	misc.c \
	mmu.c \
	mp_asm.S \
	syscache_asm.S \
	windowspill_asm.S \
	cache_asm.S \
	int_asm.S \
	interrupts.c \
	mem_ecc_parity.S \
	state_asm.S \
	state.c \
	set_region_translate.c

libhal_a_CFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_CFLAGS) \
	$(PLATFORM_INCDIR)

libhal_a_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR)
