# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2019 Intel Corporation. All rights reserved.

AC_PREREQ([2.69])
AC_INIT([rimage],[1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([src/rimage.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile
                 src/Makefile])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB(crypto, CRYPTO_new_ex_data, [], [AC_MSG_ERROR([library 'crypto' is required for OpenSSL])])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday memset])

# Where SOF header files are
AC_ARG_WITH(sof-source,
  AS_HELP_STRING([--with-sof-source=DIR],[where the root of SOF source is]),
    [ SOF_SOURCE_DIR="$with_sof_source/src" ],)
AM_CONDITIONAL([USE_PROVIDED_SOF_HEADER], [test "x$SOF_SOURCE_DIR" = "x"])

if test "x$SOF_SOURCE_DIR" != "x"; then
  SOF_INCLUDES="-I$SOF_SOURCE_DIR/include"
  CFLAGS="$CFLAGS $SOF_INCLUDES"
  CPPFLAGS="$CPPFLAGS $SOF_INCLUDES"
  AC_CHECK_HEADERS(ipc/header.h, ,
    [ AC_MSG_ERROR([Unable to find ipc/header.h. Aborting.]) ])
  AC_CHECK_HEADERS(ipc/info.h, ,
    [ AC_MSG_ERROR([Unable to find ipc/info.h. Aborting.]) ])
  AC_CHECK_HEADERS(ipc/stream.h, ,
    [ AC_MSG_ERROR([Unable to find ipc/stream.h. Aborting.]) ])
  AC_CHECK_HEADERS(kernel/fw.h, ,
    [ AC_MSG_ERROR([Unable to find kernel/fw.h. Aborting.]) ])
  AC_CHECK_HEADERS(sof/logs/file_format.h, ,
    [ AC_MSG_ERROR([Unable to find sof/logs/file_format.h. Aborting.]) ])
  AC_CHECK_HEADERS(user/manifest.h, ,
    [ AC_MSG_ERROR([Unable to find user/manifest.h. Aborting.]) ])
fi

# Generate outputs
AC_OUTPUT
