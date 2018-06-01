AUTOMAKE_OPTIONS = subdir-objects

SOF_INC = $(prefix)/include/sof
DEFINE = -DSOF_INC=\"$(SOF_INC)\"

AM_CPPFLAGS = -I $(SOF_INC) $(SOF_INCDIR) $(DEFINE)
AM_CFLAGS = -g -Wall
AM_LDFLAGS = -L$(prefix)/lib

noinst_LIBRARIES = libtb_common.a

libtb_common_a_SOURCES = \
	common_test.c \
	topology.c \
	file.c \
	trace.c \
	ipc.c \
	schedule.c \
	alloc.c
