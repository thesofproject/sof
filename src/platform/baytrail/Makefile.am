SUBDIRS = include

noinst_LIBRARIES = libplatform.a

libplatform_a_SOURCES = \
	platform.c \
	dai.c

libplatform_a_CFLAGS = \
	$(ARCH_CFLAGS) \
	$(ARCH_INCDIR) \
	$(REEF_INCDIR) \
	$(PLATFORM_INCDIR)
