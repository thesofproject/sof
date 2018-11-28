noinst_LIBRARIES = libgdb.a

libgdb_a_SOURCES = \
	init.S

libgdb_a_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)

libgdb_a_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ASFLAGS) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR) \
	$(ARCH_INCDIR) \
	$(SOF_INCDIR)
