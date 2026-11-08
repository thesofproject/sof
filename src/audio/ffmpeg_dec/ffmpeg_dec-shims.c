// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Local libc surface for the ffmpeg_dec libavcodec backend.
//
// libavcodec/libavutil reference a set of libc/libm symbols the SOF core does
// not export to LLEXT modules. This file defines them inside the module so it
// resolves at load. Categories:
//
//   * stdio / time / env (file I/O, clock, getenv, stderr): NEVER used on a SOF
//     DSP and never on the FLAC decode path -> no-op stubs. av_log()'s default
//     stderr path is additionally bypassed by the Zephyr LOG wrapper installed in
//     ffmpeg_dec-ffmpeg.c.
//   * vsnprintf / snprintf: genuinely used (av_log formatting, string utils) ->
//     a compact real implementation.
//   * str/mem helpers not exported by the core (memchr, strchr, ...): real.
//   * bsearch, __bswap*: real.
//   * strto* : Zephyr's CONFIG_MINIMAL_LIBC omits strtod/strtof and an
//     isolated LLEXT module cannot link the toolchain's non-PIC libc.a.
//     libavfilter's av_opt_set_from_string() calls strtod/strtol to parse
//     floating-point, integer, and boolean filter argument strings.
//   * libm (pow/sin/sqrt/...): FLAC is lossless integer and never calls these;
//     SOF's fixed-point math is range-limited (sqrt 0-2, exp +/-8, 16-bit sin) so
//     it cannot serve as a double libm. These are RESOLVE-ONLY STUBS that let the
//     module load. THEY MUST BE REPLACED WITH A REAL DOUBLE libm before enabling
//     any lossy codec (AAC/Opus/Vorbis), which would otherwise decode to garbage.
//
// NOTE: this file intentionally includes NO libc headers so these definitions
// cannot clash with newlib prototypes; only the symbol names matter to the linker.

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/* ============================ stdio / time / env ========================== */

/* stderr is referenced as a data object by FFmpeg's default log path. */
void *stderr;

int fprintf(void *stream, const char *fmt, ...) { (void)stream; (void)fmt; return 0; }
int fputs(const char *s, void *stream) { (void)s; (void)stream; return 0; }
unsigned long fread(void *p, unsigned long sz, unsigned long n, void *stream)
{ (void)p; (void)sz; (void)n; (void)stream; return 0; }
int fclose(void *stream) { (void)stream; return 0; }
void *fopen(const char *path, const char *mode) { (void)path; (void)mode; return NULL; }
void *fdopen(int fd, const char *mode) { (void)fd; (void)mode; return NULL; }
int setvbuf(void *s, char *buf, int mode, size_t size)
{ (void)s; (void)buf; (void)mode; (void)size; return 0; }
int open(const char *path, int flags, ...) { (void)path; (void)flags; return -1; }
int sscanf(const char *str, const char *fmt, ...) { (void)str; (void)fmt; return 0; }

char *getenv(const char *name) { (void)name; return NULL; }
unsigned long clock(void) { return 0; }
void *gmtime_r(const void *timep, void *result) { (void)timep; (void)result; return NULL; }
void *localtime(const void *timep) { (void)timep; return NULL; }
void *localtime_r(const void *timep, void *result) { (void)timep; (void)result; return NULL; }

/* iconv (avutil text/metadata charset conversion) - unused on the audio path. */
void *iconv_open(const char *to, const char *from) { (void)to; (void)from; return (void *)-1; }
int iconv_close(void *cd) { (void)cd; return 0; }
size_t iconv(void *cd, char **in, size_t *inb, char **out, size_t *outb)
{ (void)cd; (void)in; (void)inb; (void)out; (void)outb; return (size_t)-1; }
int __xpg_strerror_r(int errnum, char *buf, size_t buflen)
{ (void)errnum; if (buflen) buf[0] = '\0'; return 0; }
long mktime(void *tm) { (void)tm; return -1; }
size_t strftime(char *s, size_t max, const char *fmt, const void *tm)
{ (void)fmt; (void)tm; if (max) s[0] = '\0'; return 0; }
void abort(void) { for (;;) ; }

/* ============================ str / mem helpers =========================== */

void *memchr(const void *s, int c, size_t n)
{
	const unsigned char *p = s;

	while (n--) {
		if (*p == (unsigned char)c)
			return (void *)p;
		p++;
	}
	return NULL;
}

char *strchr(const char *s, int c)
{
	for (;; s++) {
		if (*s == (char)c)
			return (char *)s;
		if (!*s)
			return NULL;
	}
}

char *strrchr(const char *s, int c)
{
	const char *last = NULL;

	for (;; s++) {
		if (*s == (char)c)
			last = s;
		if (!*s)
			return (char *)last;
	}
}

char *strstr(const char *haystack, const char *needle)
{
	if (!*needle)
		return (char *)haystack;

	for (; *haystack; haystack++) {
		const char *h = haystack, *n = needle;

		while (*h && *n && *h == *n) {
			h++;
			n++;
		}
		if (!*n)
			return (char *)haystack;
	}
	return NULL;
}

static int ffmpeg_dec_in_set(char ch, const char *set)
{
	for (; *set; set++)
		if (*set == ch)
			return 1;
	return 0;
}

size_t strspn(const char *s, const char *accept)
{
	size_t n = 0;

	while (s[n] && ffmpeg_dec_in_set(s[n], accept))
		n++;
	return n;
}

size_t strcspn(const char *s, const char *reject)
{
	size_t n = 0;

	while (s[n] && !ffmpeg_dec_in_set(s[n], reject))
		n++;
	return n;
}

/* ============================ bsearch / bswap ============================= */

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
	      int (*compar)(const void *, const void *))
{
	size_t lo = 0, hi = nmemb;

	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		const void *elem = (const char *)base + mid * size;
		int r = compar(key, elem);

		if (r < 0)
			hi = mid;
		else if (r > 0)
			lo = mid + 1;
		else
			return (void *)elem;
	}
	return NULL;
}

uint32_t __bswapsi2(uint32_t x)
{
	return ((x & 0x000000ffu) << 24) | ((x & 0x0000ff00u) << 8) |
	       ((x & 0x00ff0000u) >> 8)  | ((x & 0xff000000u) >> 24);
}

uint64_t __bswapdi2(uint64_t x)
{
	return ((uint64_t)__bswapsi2((uint32_t)x) << 32) |
	       __bswapsi2((uint32_t)(x >> 32));
}

/* ============================ strto* ===================================== */

/*
 * Zephyr's CONFIG_MINIMAL_LIBC omits strtod/strtof (only strtol/strtoul exist
 * in zephyr/lib/libc/minimal/source/stdlib/), and an LLEXT module cannot link
 * the toolchain's non-PIC newlib libc.a or import strtod from the base image.
 * libavfilter's option parser (av_strtod) requires strtod to parse filter
 * parameter strings such as "nr=28:nf=-20" and "limit=0.95:attack=5".
 */
double strtod(const char *nptr, char **endptr)
{
	const char *s = nptr;
	double val = 0.0;
	double sign = 1.0;
	int has_digits = 0;
	int frac_exp = 0;
	int exp_val = 0;

	while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
		s++;

	if (*s == '-') {
		sign = -1.0;
		s++;
	} else if (*s == '+') {
		s++;
	}

	if ((s[0] == 'i' || s[0] == 'I') &&
	    (s[1] == 'n' || s[1] == 'N') &&
	    (s[2] == 'f' || s[2] == 'F')) {
		s += 3;
		if (endptr)
			*endptr = (char *)s;
		return sign * __builtin_inf();
	}
	if ((s[0] == 'n' || s[0] == 'N') &&
	    (s[1] == 'a' || s[1] == 'A') &&
	    (s[2] == 'n' || s[2] == 'N')) {
		s += 3;
		if (endptr)
			*endptr = (char *)s;
		return __builtin_nan("");
	}

	while (*s >= '0' && *s <= '9') {
		has_digits = 1;
		if (val < 1e18)
			val = val * 10.0 + (double)(*s - '0');
		else
			frac_exp++;
		s++;
	}

	if (*s == '.') {
		s++;
		while (*s >= '0' && *s <= '9') {
			has_digits = 1;
			if (val < 1e18) {
				val = val * 10.0 + (double)(*s - '0');
				frac_exp--;
			}
			s++;
		}
	}

	if (!has_digits) {
		if (endptr)
			*endptr = (char *)nptr;
		return 0.0;
	}

	if (*s == 'e' || *s == 'E') {
		const char *es = s + 1;
		int exp_sign = 1;
		int exp_digits = 0;

		if (*es == '-') {
			exp_sign = -1;
			es++;
		} else if (*es == '+') {
			es++;
		}
		while (*es >= '0' && *es <= '9') {
			exp_digits = 1;
			if (exp_val < 10000)
				exp_val = exp_val * 10 + (*es - '0');
			es++;
		}
		if (exp_digits) {
			exp_val *= exp_sign;
			s = es;
		} else {
			exp_val = 0;
		}
	}

	exp_val += frac_exp;
	if (val != 0.0) {
		while (exp_val >= 8) {
			val *= 1e8;
			if (val == __builtin_inf())
				break;
			exp_val -= 8;
		}
		while (exp_val > 0) {
			val *= 10.0;
			if (val == __builtin_inf())
				break;
			exp_val--;
		}
		while (exp_val <= -8) {
			val *= 1e-8;
			if (val == 0.0)
				break;
			exp_val += 8;
		}
		while (exp_val < 0) {
			val *= 0.1;
			if (val == 0.0)
				break;
			exp_val++;
		}
	}

	if (endptr)
		*endptr = (char *)s;

	return sign * val;
}

float strtof(const char *nptr, char **endptr)
{
	return (float)strtod(nptr, endptr);
}

static unsigned long long ffmpeg_dec_strtoull(const char *nptr, char **endptr,
					      int base, int *is_neg)
{
	const char *s = nptr;
	unsigned long long acc = 0;
	int neg = 0, any = 0, d;

	while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
		s++;

	if (*s == '-') {
		neg = 1;
		s++;
	} else if (*s == '+') {
		s++;
	}

	if (base < 0 || base == 1 || base > 36) {
		if (endptr)
			*endptr = (char *)nptr;
		if (is_neg)
			*is_neg = 0;
		return 0;
	}

	if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X') &&
	    ((s[2] >= '0' && s[2] <= '9') ||
	     (s[2] >= 'a' && s[2] <= 'f') ||
	     (s[2] >= 'A' && s[2] <= 'F'))) {
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = (*s == '0') ? 8 : 10;

	for (;; s++) {
		if (*s >= '0' && *s <= '9')
			d = *s - '0';
		else if (*s >= 'a' && *s <= 'z')
			d = *s - 'a' + 10;
		else if (*s >= 'A' && *s <= 'Z')
			d = *s - 'A' + 10;
		else
			break;
		if (d >= base)
			break;
		any = 1;
		acc = acc * (unsigned long long)base + (unsigned long long)d;
	}

	if (endptr)
		*endptr = (char *)(any ? s : nptr);
	if (is_neg)
		*is_neg = any ? neg : 0;
	return any ? acc : 0;
}

unsigned long long strtoull(const char *nptr, char **endptr, int base)
{
	int neg = 0;
	unsigned long long uv = ffmpeg_dec_strtoull(nptr, endptr, base, &neg);

	return neg ? -uv : uv;
}

long long strtoll(const char *nptr, char **endptr, int base)
{
	int neg = 0;
	unsigned long long uv = ffmpeg_dec_strtoull(nptr, endptr, base, &neg);

	return neg ? -(long long)uv : (long long)uv;
}

long strtol(const char *nptr, char **endptr, int base)
{
	return (long)strtoll(nptr, endptr, base);
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
	return (unsigned long)strtoull(nptr, endptr, base);
}

/* ============================ errno / libm =============================== */
/*
 * Double-precision libm is provided by the real newlib math archive
 * (libnewlib_m.a, see llext/CMakeLists.txt). newlib math functions report
 * domain/range errors through *__errno().
 */
int *__errno(void)
{
	static int llext_errno;
	return &llext_errno;
}

/* ============================ vsnprintf / snprintf ======================= */

static void ffmpeg_dec_putc(char *buf, size_t size, size_t *pos, char c)
{
	if (*pos < size)
		buf[*pos] = c;
	(*pos)++;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	size_t pos = 0;

	for (; *fmt; fmt++) {
		int left = 0, zero = 0, alt = 0, lng = 0, width = 0, prec = -1;
		unsigned long long uv = 0;
		int isnum = 0, base = 10, upper = 0, neg = 0;
		const char *s = NULL;
		char c;

		if (*fmt != '%') {
			ffmpeg_dec_putc(buf, size, &pos, *fmt);
			continue;
		}

		fmt++;
		/* flags */
		for (;; fmt++) {
			if (*fmt == '-')
				left = 1;
			else if (*fmt == '0')
				zero = 1;
			else if (*fmt == '#')
				alt = 1;
			else if (*fmt == '+' || *fmt == ' ')
				continue;
			else
				break;
		}
		/* width */
		if (*fmt == '*') {
			width = va_arg(ap, int);
			fmt++;
			if (width < 0) {
				left = 1;
				width = -width;
			}
		} else {
			while (*fmt >= '0' && *fmt <= '9')
				width = width * 10 + (*fmt++ - '0');
		}
		/* precision */
		if (*fmt == '.') {
			fmt++;
			prec = 0;
			if (*fmt == '*') {
				prec = va_arg(ap, int);
				fmt++;
			} else {
				while (*fmt >= '0' && *fmt <= '9')
					prec = prec * 10 + (*fmt++ - '0');
			}
		}
		/* length modifiers */
		for (;;) {
			if (*fmt == 'l') {
				lng++;
				fmt++;
			} else if (*fmt == 'z' || *fmt == 't' || *fmt == 'j') {
				lng = 2;
				fmt++;
			} else if (*fmt == 'h' || *fmt == 'L') {
				fmt++;
			} else {
				break;
			}
		}

		c = *fmt;
		switch (c) {
		case '%':
			ffmpeg_dec_putc(buf, size, &pos, '%');
			continue;
		case 'c':
			ffmpeg_dec_putc(buf, size, &pos, (char)va_arg(ap, int));
			continue;
		case 's':
			s = va_arg(ap, const char *);
			if (!s)
				s = "(null)";
			break;
		case 'd':
		case 'i': {
			long long v = lng >= 2 ? va_arg(ap, long long) :
				      lng == 1 ? va_arg(ap, long) : va_arg(ap, int);
			if (v < 0) {
				neg = 1;
				uv = (unsigned long long)-v;
			} else {
				uv = (unsigned long long)v;
			}
			isnum = 1;
			break;
		}
		case 'u':
			uv = lng >= 2 ? va_arg(ap, unsigned long long) :
			     lng == 1 ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
			isnum = 1;
			break;
		case 'o':
			uv = lng >= 2 ? va_arg(ap, unsigned long long) :
			     lng == 1 ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
			base = 8;
			isnum = 1;
			break;
		case 'x':
		case 'X':
			uv = lng >= 2 ? va_arg(ap, unsigned long long) :
			     lng == 1 ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
			base = 16;
			upper = (c == 'X');
			isnum = 1;
			break;
		case 'p':
			uv = (unsigned long long)(uintptr_t)va_arg(ap, void *);
			base = 16;
			alt = 1;
			isnum = 1;
			break;
		case 'e':
		case 'f':
		case 'g':
		case 'E':
		case 'F':
		case 'G':
		case 'a':
		case 'A':
			/* float formatting unsupported (see file header) */
			(void)va_arg(ap, double);
			s = "0";
			break;
		default:
			ffmpeg_dec_putc(buf, size, &pos, '%');
			ffmpeg_dec_putc(buf, size, &pos, c);
			continue;
		}

		if (isnum) {
			char digits[24];
			int di = 0, i;
			const char *hx = upper ? "0123456789ABCDEF" : "0123456789abcdef";
			int len, pad;

			if (uv == 0) {
				digits[di++] = '0';
			} else {
				while (uv) {
					digits[di++] = hx[uv % base];
					uv /= base;
				}
			}

			len = di + (neg ? 1 : 0) + (alt && base == 16 ? 2 : 0);
			pad = width > len ? width - len : 0;

			if (!left && !zero)
				while (pad-- > 0)
					ffmpeg_dec_putc(buf, size, &pos, ' ');
			if (neg)
				ffmpeg_dec_putc(buf, size, &pos, '-');
			if (alt && base == 16) {
				ffmpeg_dec_putc(buf, size, &pos, '0');
				ffmpeg_dec_putc(buf, size, &pos, upper ? 'X' : 'x');
			}
			if (!left && zero)
				while (pad-- > 0)
					ffmpeg_dec_putc(buf, size, &pos, '0');
			for (i = di - 1; i >= 0; i--)
				ffmpeg_dec_putc(buf, size, &pos, digits[i]);
			if (left)
				while (pad-- > 0)
					ffmpeg_dec_putc(buf, size, &pos, ' ');
		} else if (s) {
			int len = 0, pad;

			while (s[len] && (prec < 0 || len < prec))
				len++;
			pad = width > len ? width - len : 0;

			if (!left)
				while (pad-- > 0)
					ffmpeg_dec_putc(buf, size, &pos, ' ');
			for (int i = 0; i < len; i++)
				ffmpeg_dec_putc(buf, size, &pos, s[i]);
			if (left)
				while (pad-- > 0)
					ffmpeg_dec_putc(buf, size, &pos, ' ');
		}
	}

	if (size)
		buf[pos < size ? pos : size - 1] = '\0';

	return (int)pos;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return n;
}
