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

if BUILD_BAYTRAIL
libsof_ipc_a_SOURCES = \
	ipc.c \
	handler.c \
	byt-ipc.c \
	pmc-ipc.c \
	dma-copy.c
endif

if BUILD_CHERRYTRAIL
libsof_ipc_a_SOURCES = \
	ipc.c \
	handler.c \
	byt-ipc.c \
	pmc-ipc.c \
	dma-copy.c
endif

if BUILD_BROADWELL
libsof_ipc_a_SOURCES = \
	ipc.c \
	handler.c \
	hsw-ipc.c \
	dma-copy.c
endif

if BUILD_HASWELL
libsof_ipc_a_SOURCES = \
	ipc.c \
	handler.c \
	hsw-ipc.c \
	dma-copy.c
endif


if BUILD_APOLLOLAKE
libsof_ipc_a_SOURCES = \
	ipc.c \
	handler.c \
	apl-ipc.c \
	dma-copy.c
endif

if BUILD_CANNONLAKE
libsof_ipc_a_SOURCES = \
	ipc.c \
	handler.c \
	cnl-ipc.c \
	dma-copy.c
endif

if BUILD_SUECREEK
libsof_ipc_a_SOURCES = \
	ipc.c \
	handler.c \
	sue-ipc.c \
	dma-copy.c
endif

if BUILD_ICELAKE
libsof_ipc_a_SOURCES = \
	ipc.c \
	handler.c \
	cnl-ipc.c \
	dma-copy.c
endif

libsof_ipc_a_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(COMMON_INCDIR)
endif

