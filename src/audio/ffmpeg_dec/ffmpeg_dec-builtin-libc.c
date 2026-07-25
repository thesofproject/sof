// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Built-in (=y) libc/libm surface for the ffmpeg_dec libavcodec backend.
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
//   * double libm (acos/sin/pow/...): FLAC is lossless integer and never calls
//     these; SOF has no double libm. RESOLVE-ONLY STUBS that let the image link.
//     THEY MUST BE REPLACED WITH A REAL DOUBLE libm before enabling any lossy
//     codec (AAC/Opus/Vorbis), which would otherwise decode to garbage.
//   * __isinfd / __isnand: double classify helpers newlib would provide.
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
/* RESOLVE-ONLY double libm stubs -- see file header. FLAC never calls these. */

double acos(double x)            { (void)x; return 0; }
double asin(double x)            { (void)x; return 0; }
double atan(double x)            { (void)x; return 0; }
double atan2(double y, double x) { (void)y; (void)x; return 0; }
double ceil(double x)            { (void)x; return 0; }
double cos(double x)             { (void)x; return 0; }
double cosh(double x)            { (void)x; return 0; }
double exp(double x)             { (void)x; return 0; }
double exp2(double x)            { (void)x; return 0; }
double fabs(double x)            { return x < 0 ? -x : x; }
double floor(double x)           { (void)x; return 0; }
double fmod(double x, double y)  { (void)x; (void)y; return 0; }
double frexp(double x, int *e)   { (void)x; if (e) *e = 0; return 0; }
double hypot(double x, double y) { (void)x; (void)y; return 0; }
double log(double x)             { (void)x; return 0; }
double pow(double x, double y)   { (void)x; (void)y; return 0; }
double round(double x)           { (void)x; return 0; }
double scalbn(double x, int n)   { (void)x; (void)n; return 0; }
double sin(double x)             { (void)x; return 0; }
double sinh(double x)            { (void)x; return 0; }
double tan(double x)             { (void)x; return 0; }
double tanh(double x)            { (void)x; return 0; }
double trunc(double x)           { (void)x; return 0; }
long long llrint(double x)       { (void)x; return 0; }
int __isinfd(double x)           { (void)x; return 0; }
int __isnand(double x)           { (void)x; return 0; }

/* ============================ module heap binding ========================== */
/* No-op: the =y build allocates from common-libc malloc (DSP SRAM heap), so no
 * per-module heap binding is needed (cf. ffmpeg_dec-alloc.c in the =m build).
 */
void ffmpeg_dec_libc_bind(struct processing_module *mod) { (void)mod; }
