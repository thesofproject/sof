if BUILD_LIB
lib_LTLIBRARIES  = libsof_ipc.la

libsof_ipc_la_SOURCES = \
	ipc.c

libsof_ipc_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-version-info `echo $(VERSION) | cut -d '.' -f 1 | cut -d '0' -f 2` \
	-no-undefined \
	-export-dynamic

libsof_ipc_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(COMMON_INCDIR)
else
noinst_LIBRARIES = libsof_ipc.a

libsof_ipc_a_SOURCES = \
	ipc.c \
	handler.c \
	dma-copy.c

libsof_ipc_a_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(COMMON_INCDIR)
endif

