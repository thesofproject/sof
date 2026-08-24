// SPDX-License-Identifier: BSD-3-Clause
//
// WOV (Wake-on-Voice) capture daemon for SOF firmware.
//
// Opens a compress PCM device and loops forever capturing keyword-triggered
// audio.  Each trigger writes a time-stamped WAV file.  The VAD gate kcontrol
// is polled to detect silence and re-arm the pipeline without closing the
// compress device.
//
// Uses:
//   tinycompress  - compress PCM device API
//   raw ioctls    - ALSA kcontrol TLV read/write for VAD gate
//
// Build:  see Makefile in this directory
// Usage:  wov_capture_app [options]

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <sound/asound.h>
#include <sound/compress_params.h>
#include <tinycompress/tinycompress.h>

/* -------------------------------------------------------------------------
 * Defaults
 * ---------------------------------------------------------------------- */
#define DEFAULT_CARD          0
#define DEFAULT_DEVICE        11
#define DEFAULT_CHANNELS      1
#define DEFAULT_RATE          16000
#define DEFAULT_FRAGMENT_SZ   65536   /* bytes per fragment */
#define DEFAULT_FRAGMENTS     8
#define DEFAULT_OUT_DIR       "/tmp"
#define DEFAULT_VAD_THRESHOLD 0       /* 0 = gate always open */
#define DEFAULT_VAD_ONSET     5
#define DEFAULT_VAD_HANGOVER  100
#define POLL_INTERVAL_MS      200     /* VAD status poll rate */
#define DRAIN_IDLE_MS         600     /* silence after last bytes → drain done */
#define DRAIN_TIMEOUT_MS      6000    /* hard timeout for drain */
#define CTL_DEV_FMT           "/dev/snd/controlC%u"

/* -------------------------------------------------------------------------
 * VAD kcontrol numids (pipeline 100 topology)
 * ---------------------------------------------------------------------- */
#define VAD_CFG_NUMID     8    /* vad_gate_cfg_100    R/W binary */
#define VAD_STATUS_NUMID  9    /* vad_gate_status_100 RO volatile */

/* SOF IPC4 ABI */
#define SOF_IPC4_ABI_MAGIC    0x34464F53u
#define SOF_CTRL_CMD_BINARY   3
#define ABI_HDR_SIZE          32
#define VAD_CFG_PAYLOAD_SZ    12   /* threshold(4) onset(2) hangover(2) shift(1) pad(3) */
#define VAD_STATUS_PAYLOAD_SZ  8   /* energy(4) vad_active(1) pad(3) */
#define PARAM_SET_CFG         1
#define PARAM_GET_STATUS      2

/* ioctl codes */
#define TLV_READ_IOC  0xC008551Au   /* SNDRV_CTL_IOCTL_TLV_READ  */
#define TLV_WRITE_IOC 0xC008551Bu   /* SNDRV_CTL_IOCTL_TLV_WRITE */

/* -------------------------------------------------------------------------
 * WAV header
 * ---------------------------------------------------------------------- */
struct wav_header {
	/* RIFF chunk */
	char     riff[4];        /* "RIFF" */
	uint32_t file_size;      /* total - 8 */
	char     wave[4];        /* "WAVE" */
	/* fmt sub-chunk */
	char     fmt[4];         /* "fmt " */
	uint32_t fmt_size;       /* 16 */
	uint16_t audio_format;   /* 1=PCM */
	uint16_t channels;
	uint32_t sample_rate;
	uint32_t byte_rate;      /* sample_rate * channels * bits/8 */
	uint16_t block_align;    /* channels * bits/8 */
	uint16_t bits_per_sample;
	/* data sub-chunk */
	char     data[4];        /* "data" */
	uint32_t data_size;
} __attribute__((packed));

/* -------------------------------------------------------------------------
 * Application state
 * ---------------------------------------------------------------------- */
struct wov_state {
	unsigned int card;
	unsigned int device;
	unsigned int channels;
	unsigned int rate;
	unsigned int frag_sz;
	unsigned int fragments;
	const char  *out_dir;
	int          max_cycles;    /* 0 = unlimited */
	int32_t      vad_threshold;
	uint16_t     vad_onset;
	uint16_t     vad_hangover;
	int          verbose;
	volatile bool stop;         /* set by SIGINT */
};

/* -------------------------------------------------------------------------
 * Logging
 * ---------------------------------------------------------------------- */
static void log_ts(const char *level, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

static void log_ts(const char *level, const char *fmt, ...)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	struct tm *tm = localtime(&ts.tv_sec);
	char tbuf[32];
	strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);

	fprintf(stderr, "[%s.%03ld] [%s] ", tbuf,
		ts.tv_nsec / 1000000L, level);

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	fflush(stderr);
}

#define log_info(...)  log_ts("INFO ", __VA_ARGS__)
#define log_state(...) log_ts("STATE", __VA_ARGS__)
#define log_ctl(...)   log_ts("CTL  ", __VA_ARGS__)
#define log_err(...)   log_ts("ERROR", __VA_ARGS__)
#define log_dbg(s, ...) do { if ((s)->verbose) log_ts("DEBUG", __VA_ARGS__); } while (0)

/* -------------------------------------------------------------------------
 * VAD kcontrol helpers
 * ---------------------------------------------------------------------- */
static int ctl_open(unsigned int card)
{
	char path[64];
	snprintf(path, sizeof(path), CTL_DEV_FMT, card);
	int fd = open(path, O_RDWR);
	if (fd < 0)
		log_err("open %s: %s", path, strerror(errno));
	return fd;
}

/* Read vad_gate_status_100: returns 0 on success, fills energy + active */
static int vad_read_status(unsigned int card, uint32_t *energy, bool *active)
{
	/* Buffer layout (56 bytes):
	 * [0..7]  outer tlv: numid, length=48
	 * [8..15] inner tlv: filled by kernel
	 * [16..47] sof_abi_hdr (32 bytes)
	 * [48..55] status payload: energy(u32) + active(u8) + pad(3)
	 */
	uint32_t buf[14] = { 0 };   /* 56 bytes */
	buf[0] = VAD_STATUS_NUMID;
	buf[1] = 8 + ABI_HDR_SIZE + VAD_STATUS_PAYLOAD_SZ;  /* 48 */

	int fd = ctl_open(card);
	if (fd < 0)
		return -1;
	int rc = ioctl(fd, TLV_READ_IOC, buf);
	close(fd);
	if (rc < 0)
		return -1;

	uint32_t magic = buf[4];  /* offset 16 */
	if (magic != SOF_IPC4_ABI_MAGIC)
		return -1;

	/* offset 48 = index 12 */
	*energy = buf[12];
	*active = (bool)(((uint8_t *)buf)[52]);
	return 0;
}

/* Write vad_gate_cfg_100 */
static int vad_write_config(const struct wov_state *s)
{
	/* Buffer layout (60 bytes):
	 * [0..7]  outer tlv: numid=CFG_NUMID, length=52
	 * [8..15] inner tlv: SOF_CTRL_CMD_BINARY, length=44
	 * [16..47] sof_abi_hdr
	 * [48..59] config payload: threshold(i32) onset(u16) hangover(u16) shift(u8) pad(3)
	 */
	uint8_t buf[60] = { 0 };

	/* outer tlv */
	uint32_t *u32 = (uint32_t *)buf;
	u32[0] = VAD_CFG_NUMID;
	u32[1] = 8 + ABI_HDR_SIZE + VAD_CFG_PAYLOAD_SZ;    /* 52 */

	/* inner tlv */
	u32[2] = SOF_CTRL_CMD_BINARY;
	u32[3] = ABI_HDR_SIZE + VAD_CFG_PAYLOAD_SZ;         /* 44 */

	/* sof_abi_hdr at offset 16 */
	u32[4] = SOF_IPC4_ABI_MAGIC;
	u32[5] = PARAM_SET_CFG;
	u32[6] = VAD_CFG_PAYLOAD_SZ;
	u32[7] = 0;                   /* abi version */
	/* u32[8..11] = reserved 16 bytes → already zero */

	/* payload at offset 48 */
	int32_t *i32 = (int32_t *)(buf + 48);
	i32[0] = s->vad_threshold;
	uint16_t *u16 = (uint16_t *)(buf + 52);
	u16[0] = s->vad_onset;
	u16[1] = s->vad_hangover;
	buf[56] = 0;  /* shift */

	int fd = ctl_open(s->card);
	if (fd < 0)
		return -1;
	int rc = ioctl(fd, TLV_WRITE_IOC, buf);
	close(fd);
	if (rc < 0) {
		log_err("TLV_WRITE vad_gate_cfg: %s", strerror(errno));
		return -1;
	}
	return 0;
}

/* -------------------------------------------------------------------------
 * WAV file helpers
 * ---------------------------------------------------------------------- */
static void wav_fill_header(struct wav_header *h, unsigned int channels,
			    unsigned int rate, uint32_t data_bytes)
{
	unsigned int bits = 32;
	unsigned int blk  = channels * (bits / 8);

	memcpy(h->riff, "RIFF", 4);
	h->file_size     = data_bytes + sizeof(*h) - 8;
	memcpy(h->wave, "WAVE", 4);
	memcpy(h->fmt,  "fmt ", 4);
	h->fmt_size      = 16;
	h->audio_format  = 1;   /* PCM */
	h->channels      = channels;
	h->sample_rate   = rate;
	h->byte_rate     = rate * blk;
	h->block_align   = blk;
	h->bits_per_sample = bits;
	memcpy(h->data, "data", 4);
	h->data_size     = data_bytes;
}

static char *wav_filename(const char *dir, int cycle_id)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	struct tm *tm = localtime(&ts.tv_sec);

	char path[256];
	snprintf(path, sizeof(path), "%s/wov_%04d%02d%02d_%02d%02d%02d_%03d.wav",
		 dir,
		 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		 tm->tm_hour, tm->tm_min, tm->tm_sec,
		 cycle_id);
	return strdup(path);
}

/* Write WAV with placeholder header, return open FILE* at data start */
static FILE *wav_open(const char *path, unsigned int channels, unsigned int rate)
{
	FILE *f = fopen(path, "wb");
	if (!f) {
		log_err("fopen %s: %s", path, strerror(errno));
		return NULL;
	}
	struct wav_header h = { 0 };
	wav_fill_header(&h, channels, rate, 0);  /* placeholder data_size */
	fwrite(&h, sizeof(h), 1, f);
	return f;
}

/* Rewind and patch data_size + file_size in WAV header */
static void wav_finalize(FILE *f, uint32_t data_bytes, unsigned int channels,
			 unsigned int rate)
{
	struct wav_header h = { 0 };
	wav_fill_header(&h, channels, rate, data_bytes);
	rewind(f);
	fwrite(&h, sizeof(h), 1, f);
	fflush(f);
}

/* -------------------------------------------------------------------------
 * Compress device setup
 * ---------------------------------------------------------------------- */
static struct compress *compress_setup(const struct wov_state *s)
{
	struct snd_codec codec = {
		.id          = SND_AUDIOCODEC_PCM,
		.ch_in       = s->channels,
		.ch_out      = s->channels,
		.sample_rate = s->rate,
		.format      = SNDRV_PCM_FORMAT_S32_LE,
	};
	struct compr_config cfg = {
		.fragment_size = s->frag_sz,
		.fragments     = s->fragments,
		.codec         = &codec,
	};

	struct compress *c = compress_open(s->card, s->device,
					   COMPRESS_OUT, &cfg);
	if (!c || !is_compress_ready(c)) {
		log_err("compress_open card=%u device=%u: %s",
			s->card, s->device,
			c ? compress_get_error(c) : strerror(errno));
		if (c)
			compress_close(c);
		return NULL;
	}
	return c;
}

/* -------------------------------------------------------------------------
 * One WOV capture cycle
 * ---------------------------------------------------------------------- */
enum cycle_result {
	CYCLE_OK = 0,
	CYCLE_NO_TRIGGER,  /* idle timeout before trigger */
	CYCLE_ERROR,
};

static enum cycle_result run_cycle(const struct wov_state *s,
				   struct compress *compress,
				   int cycle_id,
				   uint64_t *bytes_out)
{
	*bytes_out = 0;

	uint8_t *buf = malloc(s->frag_sz);
	if (!buf) {
		log_err("malloc %u: %s", s->frag_sz, strerror(errno));
		return CYCLE_ERROR;
	}

	log_state("cycle %d: WAITING for WOV trigger", cycle_id);

	bool     triggered    = false;
	bool     silence_seen = false;
	uint64_t total_bytes  = 0;
	struct timespec last_data_ts;
	struct timespec drain_start_ts;
	clock_gettime(CLOCK_REALTIME, &last_data_ts);

	FILE    *wav_file  = NULL;
	char    *wav_path  = NULL;

	uint32_t prev_energy = UINT32_MAX;
	bool     prev_active = false;
	bool     first_status = true;

	enum cycle_result result = CYCLE_NO_TRIGGER;

	while (!s->stop) {
		/* Wait up to POLL_INTERVAL_MS for data, then check VAD status.
		 * compress_wait returns 0=data_ready, -1=timeout(ETIME) or error. */
		int ret = compress_wait(compress, POLL_INTERVAL_MS);
		if (ret < 0 && errno != ETIME) {
			log_err("compress_wait: %s", compress_get_error(compress));
			result = CYCLE_ERROR;
			break;
		}

		if (ret == 0) {
			int n = compress_read(compress, buf, s->frag_sz);
			if (n > 0) {
				if (!triggered) {
					triggered = true;
					wav_path  = wav_filename(s->out_dir, cycle_id);
					wav_file  = wav_open(wav_path, s->channels, s->rate);
					if (!wav_file) {
						result = CYCLE_ERROR;
						break;
					}
					log_state("cycle %d: TRIGGERED → writing %s",
						  cycle_id, wav_path);
					result = CYCLE_OK;
				}
				fwrite(buf, 1, n, wav_file);
				total_bytes += n;
				clock_gettime(CLOCK_REALTIME, &last_data_ts);
			}
		}

		/* Poll VAD kcontrol */
		uint32_t energy = 0;
		bool     active = false;
		if (vad_read_status(s->card, &energy, &active) == 0) {
			bool changed = first_status ||
				       energy != prev_energy ||
				       active != prev_active;
			if (changed) {
				log_ctl("vad_gate_status_100: energy=%u vad_active=%s",
					energy, active ? "true" : "false");
				if (!first_status && active != prev_active) {
					log_state("cycle %d: VAD %s",
						  cycle_id,
						  active ? "SPEECH" : "SILENCE");
				}
				prev_energy = energy;
				prev_active = active;
				first_status = false;
			}
			if (triggered && !silence_seen && !active) {
				log_state("cycle %d: drain start (energy=%u)",
					  cycle_id, energy);
				silence_seen = true;
				clock_gettime(CLOCK_REALTIME, &drain_start_ts);
			}
		}

		/* Drain completion check */
		if (silence_seen) {
			struct timespec now;
			clock_gettime(CLOCK_REALTIME, &now);

			long idle_ms  = (now.tv_sec - last_data_ts.tv_sec) * 1000L +
					(now.tv_nsec - last_data_ts.tv_nsec) / 1000000L;
			long drain_ms = (now.tv_sec - drain_start_ts.tv_sec) * 1000L +
					(now.tv_nsec - drain_start_ts.tv_nsec) / 1000000L;

			if (idle_ms >= DRAIN_IDLE_MS) {
				log_state("cycle %d: DRAIN_COMPLETE (%ld ms idle, "
					  "%" PRIu64 " bytes)", cycle_id, idle_ms, total_bytes);
				break;
			}
			if (drain_ms >= DRAIN_TIMEOUT_MS) {
				log_state("cycle %d: DRAIN_TIMEOUT (%ld ms, "
					  "%" PRIu64 " bytes)", cycle_id, drain_ms, total_bytes);
				break;
			}
		}

		/* No trigger yet: check idle timeout (3 × DRAIN_TIMEOUT as guard) */
		if (!triggered) {
			struct timespec now;
			clock_gettime(CLOCK_REALTIME, &now);
			long idle_ms = (now.tv_sec - last_data_ts.tv_sec) * 1000L +
				       (now.tv_nsec - last_data_ts.tv_nsec) / 1000000L;
			if (idle_ms > 3 * DRAIN_TIMEOUT_MS) {
				log_state("cycle %d: IDLE_TIMEOUT — no trigger",
					  cycle_id);
				result = CYCLE_NO_TRIGGER;
				break;
			}
		}
	}

	if (wav_file) {
		wav_finalize(wav_file, (uint32_t)total_bytes, s->channels, s->rate);
		fclose(wav_file);
		log_info("cycle %d: saved %s (%" PRIu64 " bytes = %.2f s)",
			 cycle_id, wav_path, total_bytes,
			 (double)total_bytes / (s->rate * s->channels * 4));
	}
	free(wav_path);
	free(buf);
	*bytes_out = total_bytes;
	return result;
}

/* -------------------------------------------------------------------------
 * Signal handler
 * ---------------------------------------------------------------------- */
static struct wov_state *g_state;

static void sigint_handler(int sig)
{
	(void)sig;
	if (g_state)
		g_state->stop = true;
}

/* -------------------------------------------------------------------------
 * Usage / main
 * ---------------------------------------------------------------------- */
static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [options]\n"
		"\n"
		"  -c CARD        sound card number (default %d)\n"
		"  -d DEVICE      compress PCM device (default %d)\n"
		"  -n CYCLES      number of WOV cycles, 0=unlimited (default 0)\n"
		"  -o DIR         output directory for WAV files (default %s)\n"
		"  -t THRESHOLD   VAD gate threshold in raw S32 energy units\n"
		"                 0=always open (default), 300000000=lab ambient\n"
		"  -r RATE        sample rate Hz (default %d)\n"
		"  -f FRAG_SZ     fragment size bytes (default %d)\n"
		"  -v             verbose debug logging\n"
		"  -h             this help\n"
		"\n"
		"Each WOV trigger writes: DIR/wov_YYYYMMDD_HHMMSS_NNN.wav\n"
		"\n"
		"Example — unlimited capture, gate at lab threshold:\n"
		"  %s -o /tmp -t 300000000\n"
		"\n"
		"State log tags: STATE=pipeline transitions, CTL=kcontrol changes\n",
		prog,
		DEFAULT_CARD, DEFAULT_DEVICE, DEFAULT_OUT_DIR,
		DEFAULT_RATE, DEFAULT_FRAGMENT_SZ,
		prog);
}

int main(int argc, char *argv[])
{
	struct wov_state s = {
		.card          = DEFAULT_CARD,
		.device        = DEFAULT_DEVICE,
		.channels      = DEFAULT_CHANNELS,
		.rate          = DEFAULT_RATE,
		.frag_sz       = DEFAULT_FRAGMENT_SZ,
		.fragments     = DEFAULT_FRAGMENTS,
		.out_dir       = DEFAULT_OUT_DIR,
		.max_cycles    = 0,
		.vad_threshold = DEFAULT_VAD_THRESHOLD,
		.vad_onset     = DEFAULT_VAD_ONSET,
		.vad_hangover  = DEFAULT_VAD_HANGOVER,
		.verbose       = 0,
		.stop          = false,
	};
	g_state = &s;

	int opt;
	while ((opt = getopt(argc, argv, "c:d:n:o:t:r:f:vh")) != -1) {
		switch (opt) {
		case 'c': s.card          = (unsigned int)atoi(optarg); break;
		case 'd': s.device        = (unsigned int)atoi(optarg); break;
		case 'n': s.max_cycles    = atoi(optarg);               break;
		case 'o': s.out_dir       = optarg;                     break;
		case 't': s.vad_threshold = atoi(optarg);               break;
		case 'r': s.rate          = (unsigned int)atoi(optarg); break;
		case 'f': s.frag_sz       = (unsigned int)atoi(optarg); break;
		case 'v': s.verbose       = 1;                          break;
		case 'h': usage(argv[0]); return 0;
		default:  usage(argv[0]); return 1;
		}
	}

	signal(SIGINT,  sigint_handler);
	signal(SIGTERM, sigint_handler);

	log_info("wov_capture_app starting: card=%u device=%u rate=%u "
		 "channels=%u frag=%u cycles=%s",
		 s.card, s.device, s.rate, s.channels, s.frag_sz,
		 s.max_cycles ? "limited" : "unlimited");

	/* Open compress device once and keep open for all cycles */
	struct compress *compress = compress_setup(&s);
	if (!compress)
		return 1;
	log_state("compress: OPEN card=%u device=%u", s.card, s.device);

	if (compress_start(compress) != 0) {
		log_err("compress_start: %s", compress_get_error(compress));
		compress_close(compress);
		return 1;
	}
	log_state("compress: RUNNING");

	/* Write initial VAD threshold if non-zero */
	if (s.vad_threshold != 0) {
		if (vad_write_config(&s) == 0)
			log_ctl("vad_gate_cfg_100 write: threshold=%d onset=%u "
				"hangover=%u shift=0",
				s.vad_threshold, s.vad_onset, s.vad_hangover);
		else
			log_err("vad_write_config failed (pipeline not yet active?)");
	}

	log_info("output dir: %s  threshold=%d", s.out_dir, s.vad_threshold);

	int      cycle     = 0;
	uint64_t total_b   = 0;
	bool     unlimited = (s.max_cycles == 0);

	while (!s.stop && (unlimited || cycle < s.max_cycles)) {
		cycle++;
		log_state("cycle %d: REARM", cycle);

		uint64_t bytes = 0;
		enum cycle_result r = run_cycle(&s, compress, cycle, &bytes);
		total_b += bytes;

		if (r == CYCLE_ERROR) {
			log_err("cycle %d: fatal error", cycle);
			break;
		}
		if (r == CYCLE_NO_TRIGGER) {
			log_state("cycle %d: no trigger, stopping", cycle);
			break;
		}

		if (!unlimited && cycle >= s.max_cycles)
			break;

		/* Brief pause before re-arming */
		usleep(300000);
	}

	compress_stop(compress);
	log_state("compress: STOP");
	compress_close(compress);
	log_state("compress: CLOSE");

	log_info("done: %d cycle(s), %" PRIu64 " bytes total", cycle, total_b);
	return 0;
}
