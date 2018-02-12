SUBDIRS = src

ACLOCAL_AMFLAGS = -I m4

SRC_DIR = $(abs_top_builddir)/src

export REEF_INCDIR = \
	-I $(SRC_DIR)/include \
	-I $(ROOT_DIR)/include

export ARCH_INCDIR = \
	-I $(SRC_DIR)/arch/$(ARCH)/include \
	-I $(SRC_DIR)/arch/$(ARCH)/xtos

export PLATFORM_INCDIR = \
	-I $(SRC_DIR)/platform/$(PLATFORM)/include

BUILT_SOURCES = $(top_srcdir)/src/include/version.h

$(top_srcdir)/src/include/version.h: Makefile
	./git-version.sh

dist-hook:
	./git-version.sh
	cat .version > $(distdir)/.tarball-version
	cat .version > $(distdir)/.version

all-local:
	rm -f $(top_srcdir)/src/include/version.h
