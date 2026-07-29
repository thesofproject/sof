// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Built-in (=y) libc surface for the ffmpeg_dec libavcodec backend.
//
// The LLEXT (=m) build uses ffmpeg_dec-shims.c, which defines a full private
// libc (string/stdio/stdlib/snprintf/...) because an isolated module cannot
// import the core's libc. A built-in (=y) image links against Zephyr's common
// libc, which already provides malloc/free/calloc/realloc (a real sys_heap over
// all remaining SRAM on Intel ADSP), snprintf, memcpy, str*, strto*, bsearch,
// etc. -- all whole-archived. Re-defining those here would multiply-define.
//
// So this file provides ONLY the symbols libavcodec/libavutil reference that
// Zephyr's common/minimal libc does NOT export, and nothing else. These are the
// exact unresolved references from the base-image link:
//
//   * stdio / time / env (stderr, fread, fclose, fdopen, open, setvbuf, sscanf,
//     getenv, clock, gmtime, mktime, strftime): NEVER used on a SOF DSP nor on
//     the FLAC decode path -> no-op stubs. av_log()'s stderr path is bypassed by
//     the Zephyr LOG wrapper installed in ffmpeg_dec-ffmpeg.c.
//   * strtod: not on the FLAC decode path -> return-0 stub.
//   * __errno: the double libm libav* references (acos/sin/exp/log/pow/scalbn/
//     __isinfd/...) is NOT stubbed here -- it is the real newlib math for this
//     core, compiled from vendored newlib source with -mlongcalls into
//     FFMPEG_TARGET_LIBM and linked in CMakeLists.txt (see ffmpeg.cmake 3c +
//     libm/README). Those kernels report domain/range
//     errors through *__errno(); it is the only libc-side symbol they need that
//     the image doesn't already provide (the soft-float builtins __adddf3/
//     __muldf3/... and memset are already linked), so bridge it to Zephyr's real
//     per-thread errno below. This replaces the former return-0 libm stubs, so
//     lossy codecs (AAC/Opus/Vorbis) now decode with correct math, not garbage.
//   * z_errno_wrap: a Zephyr libc runtime errno wrapper referenced by
//     libavutil/avsscanf; harmless stub (errno unused on the decode path).
//   * ffmpeg_dec_libc_bind: in the =y build av_malloc() bottoms out in the
//     common-libc malloc backed by the DSP SRAM heap, so no per-module heap
//     binding is needed -> no-op (the =m build routes it to the module heap in
//     ffmpeg_dec-alloc.c).
//
// As in ffmpeg_dec-shims.c, this file intentionally includes NO libc headers so
// these definitions cannot clash with newlib prototypes; only the symbol names
// matter to the linker.

#include <stddef.h>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wincompatible-library-redeclaration"
#pragma clang diagnostic ignored "-Wbuiltin-requires-header"
#endif

struct processing_module;

/* ============================ stdio / time / env ========================== */

/* stderr is referenced as a data object by FFmpeg's default log path. */
void *stderr;

unsigned long fread(void *p, unsigned long sz, unsigned long n, void *stream)
{ (void)p; (void)sz; (void)n; (void)stream; return 0; }
int fclose(void *stream) { (void)stream; return 0; }
void *fdopen(int fd, const char *mode) { (void)fd; (void)mode; return NULL; }
int open(const char *path, int flags, ...) { (void)path; (void)flags; return -1; }
int setvbuf(void *s, char *buf, int mode, size_t size)
{ (void)s; (void)buf; (void)mode; (void)size; return 0; }
int sscanf(const char *str, const char *fmt, ...) { (void)str; (void)fmt; return 0; }
char *getenv(const char *name) { (void)name; return NULL; }
unsigned long clock(void) { return 0; }
void *gmtime(const void *timep) { (void)timep; return NULL; }
long mktime(void *tm) { (void)tm; return -1; }
size_t strftime(char *s, size_t max, const char *fmt, const void *tm)
{ (void)fmt; (void)tm; if (max) s[0] = '\0'; return 0; }
double strtod(const char *nptr, char **endptr)
{ if (endptr) *endptr = (char *)nptr; return 0; }

/* Zephyr libc runtime errno wrapper referenced by libavutil/avsscanf. */
int z_errno_wrap(void) { return 0; }

/* ================================== libm =================================== */
/*
 * The double libm itself is the real newlib libm.a for this core (linked in
 * CMakeLists.txt) -- no math stubs live here. newlib's kernels record domain/
 * range errors through *__errno(); it is the only libm-side symbol the base image
 * lacks. Bridge it to Zephyr's genuine per-thread errno (z_impl_z_errno, the
 * implementation behind the z_errno syscall) so those writes land in the calling
 * thread's errno rather than a shared dummy. The decode path never reads errno,
 * but the reference must resolve for libm.a to link.
 */
extern int *z_impl_z_errno(void);
int *__errno(void) { return z_impl_z_errno(); }

/* ffmpeg_dec_libc_bind() + the malloc/free/realloc/calloc heap routing live in
 * ffmpeg_dec-builtin-alloc.c (the =y build --wraps them onto the SOF DSP heap).
 */
