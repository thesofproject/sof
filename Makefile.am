if BUILD_RIMAGE
SUBDIRS = rimage
else
SUBDIRS = doc src
endif

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = version.sh

SRC_DIR = $(abs_top_builddir)/src

if BUILD_HOST
export ARCH_INCDIR = \
	-I $(SRC_DIR)/arch/$(ARCH)/include

export SOF_INCDIR = \
	-I $(SRC_DIR)/include

if BUILD_LIB
export PLATFORM_INCDIR = \
	-I $(SRC_DIR)/library/include
endif

else
export SOF_INCDIR = \
	-I $(SRC_DIR)/include \
	-I $(ROOT_DIR)/include

export ARCH_INCDIR = \
	-I $(SRC_DIR)/arch/$(ARCH)/include \
	-I $(SRC_DIR)/arch/$(ARCH)/xtos
export PLATFORM_INCDIR = \
	-I $(SRC_DIR)/platform/$(PLATFORM)/include
endif

dist-hook:
	./version.sh $(top_srcdir)
	cat .version > $(distdir)/.tarball-version
	cat .version > $(distdir)/.version

doc-dummy:

doc: doc-dummy
	$(MAKE) -C doc doc

all-local:
	rm -f $(top_srcdir)/src/include/version.h
