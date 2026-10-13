// SPDX-License-Identifier: BSD-3-Clause
//
// WOV (Wake-on-Voice) Regular PCM Capture Application for SOF Firmware.
//
// Captures audio from SOF regular PCM devices (e.g. hw:0,11 - 16 kHz, 2-channel
// S16_LE) without requiring external library dependencies (uses direct Linux
// ALSA PCM kernel UAPI ioctls).
//
// Features:
//   - Zero-dependency compilation (standard GCC + linux/sound/asound.h)
//   - Standard RIFF WAVE file output with correct headers
//   - Multi-channel real-time energy & RMS statistics calculation
//   - Multi-cycle capture loop for repeatability and driver stability testing
//   - Graceful signal handling (SIGINT/SIGTERM cleanly finalizes WAV headers)
//
// Build:  make
// Usage:  wov_capture_app [options]

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
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

/* -------------------------------------------------------------------------
 * Defaults
 * ---------------------------------------------------------------------- */
#define DEFAULT_CARD          0
#define DEFAULT_DEVICE        11
#define DEFAULT_CHANNELS      2
#define DEFAULT_RATE          16000
#define DEFAULT_FORMAT        SNDRV_PCM_FORMAT_S16_LE
#define DEFAULT_PERIOD_FRAMES 1024
#define DEFAULT_PERIODS       4
#define DEFAULT_DURATION_S    3.0
#define DEFAULT_CYCLES        1
#define DEFAULT_OUT_DIR       "/tmp"
#define PCM_DEV_FMT           "/dev/snd/pcmC%uD%uc"

/* -------------------------------------------------------------------------
 * WAV Header Structure
 * ---------------------------------------------------------------------- */
struct wav_header {
	/* RIFF Chunk */
	char     riff[4];        /* "RIFF" */
	uint32_t file_size;      /* total size - 8 bytes */
	char     wave[4];        /* "WAVE" */
	/* fmt Sub-chunk */
	char     fmt[4];         /* "fmt " */
	uint32_t fmt_size;       /* 16 for PCM */
	uint16_t audio_format;   /* 1 = PCM */
	uint16_t channels;
	uint32_t sample_rate;
	uint32_t byte_rate;      /* sample_rate * channels * (bits_per_sample / 8) */
	uint16_t block_align;    /* channels * (bits_per_sample / 8) */
	uint16_t bits_per_sample;
	/* data Sub-chunk */
	char     data[4];        /* "data" */
	uint32_t data_size;      /* bytes of audio data */
} __attribute__((packed));

/* -------------------------------------------------------------------------
 * Channel Statistics
 * ---------------------------------------------------------------------- */
struct channel_stats {
	int64_t sum;
	int64_t sum_sq;
	int64_t min_val;
	int64_t max_val;
	uint64_t sample_count;
};

/* -------------------------------------------------------------------------
 * Application Configuration & State
 * ---------------------------------------------------------------------- */
struct app_config {
	unsigned int card;
	unsigned int device;
	unsigned int channels;
	unsigned int rate;
	int          format;            /* SNDRV_PCM_FORMAT_S16_LE or S32_LE */
	unsigned int period_frames;
	unsigned int periods;
	double       duration_s;        /* 0 = unlimited */
	int          cycles;            /* 0 = unlimited */
	double       cycle_delay_s;
	const char  *out_dir;
	const char  *out_file;
	int          verbose;
};

static volatile bool g_stop = false;

static void sig_handler(int sig)
{
	(void)sig;
	g_stop = true;
}

/* -------------------------------------------------------------------------
 * Logging Helper
 * ---------------------------------------------------------------------- */
static void log_msg(const char *tag, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

static void log_msg(const char *tag, const char *fmt, ...)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	struct tm *tm = localtime(&ts.tv_sec);
	char tbuf[32];
	strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);

	fprintf(stderr, "[%s.%03ld] [%-5s] ", tbuf, ts.tv_nsec / 1000000L, tag);

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	fflush(stderr);
}

#define log_info(...)  log_msg("INFO", __VA_ARGS__)
#define log_state(...) log_msg("STATE", __VA_ARGS__)
#define log_err(...)   log_msg("ERROR", __VA_ARGS__)
#define log_dbg(cfg, ...) do { if ((cfg)->verbose) log_msg("DEBUG", __VA_ARGS__); } while (0)

/* -------------------------------------------------------------------------
 * ALSA Raw PCM ioctl Helpers
 * ---------------------------------------------------------------------- */
static inline void set_interval(struct snd_interval *i, unsigned int min, unsigned int max)
{
	i->min = min;
	i->max = max;
	i->openmin = 0;
	i->openmax = 0;
	i->integer = 1;
	i->empty = 0;
}

static inline void set_mask(struct snd_mask *m, unsigned int val)
{
	memset(m, 0, sizeof(*m));
	if (val < SNDRV_MASK_MAX)
		m->bits[val / 32] |= (1u << (val % 32));
}

static void hw_param_init(struct snd_pcm_hw_params *params)
{
	int i;
	memset(params, 0, sizeof(*params));
	for (i = 0; i <= SNDRV_PCM_HW_PARAM_LAST_MASK - SNDRV_PCM_HW_PARAM_FIRST_MASK; i++)
		memset(&params->masks[i], 0xff, sizeof(struct snd_mask));
	for (i = 0; i <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL; i++) {
		params->intervals[i].min = 0;
		params->intervals[i].max = UINT_MAX;
		params->intervals[i].openmin = 0;
		params->intervals[i].openmax = 0;
		params->intervals[i].integer = 0;
		params->intervals[i].empty = 0;
	}
	params->rmask = ~0U;
	params->cmask = 0;
	params->info = ~0U;
}

static int pcm_configure(int fd, const struct app_config *cfg,
                         unsigned int *out_period_frames,
                         unsigned int *out_buffer_frames)
{
	struct snd_pcm_hw_params hw;
	struct snd_pcm_sw_params sw;
	int ret;

	hw_param_init(&hw);

	ret = ioctl(fd, SNDRV_PCM_IOCTL_HW_REFINE, &hw);
	if (ret < 0) {
		log_err("SNDRV_PCM_IOCTL_HW_REFINE failed: %s (errno %d)",
		        strerror(errno), errno);
		return -errno;
	}

	set_mask(&hw.masks[SNDRV_PCM_HW_PARAM_ACCESS - SNDRV_PCM_HW_PARAM_FIRST_MASK],
	         SNDRV_PCM_ACCESS_RW_INTERLEAVED);
	set_mask(&hw.masks[SNDRV_PCM_HW_PARAM_FORMAT - SNDRV_PCM_HW_PARAM_FIRST_MASK],
	         cfg->format);
	set_mask(&hw.masks[SNDRV_PCM_HW_PARAM_SUBFORMAT - SNDRV_PCM_HW_PARAM_FIRST_MASK],
	         SNDRV_PCM_SUBFORMAT_STD);

	set_interval(&hw.intervals[SNDRV_PCM_HW_PARAM_CHANNELS - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL],
	             cfg->channels, cfg->channels);
	set_interval(&hw.intervals[SNDRV_PCM_HW_PARAM_RATE - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL],
	             cfg->rate, cfg->rate);

	if (cfg->period_frames > 0) {
		set_interval(&hw.intervals[SNDRV_PCM_HW_PARAM_PERIOD_SIZE - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL],
		             cfg->period_frames, cfg->period_frames);
	}
	if (cfg->periods > 0) {
		set_interval(&hw.intervals[SNDRV_PCM_HW_PARAM_PERIODS - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL],
		             cfg->periods, cfg->periods);
	}

	ret = ioctl(fd, SNDRV_PCM_IOCTL_HW_PARAMS, &hw);
	if (ret < 0) {
		log_dbg(cfg, "Retrying HW_PARAMS with driver-selected period/buffer sizes...");
		hw_param_init(&hw);
		set_mask(&hw.masks[SNDRV_PCM_HW_PARAM_ACCESS - SNDRV_PCM_HW_PARAM_FIRST_MASK],
		         SNDRV_PCM_ACCESS_RW_INTERLEAVED);
		set_mask(&hw.masks[SNDRV_PCM_HW_PARAM_FORMAT - SNDRV_PCM_HW_PARAM_FIRST_MASK],
		         cfg->format);
		set_mask(&hw.masks[SNDRV_PCM_HW_PARAM_SUBFORMAT - SNDRV_PCM_HW_PARAM_FIRST_MASK],
		         SNDRV_PCM_SUBFORMAT_STD);
		set_interval(&hw.intervals[SNDRV_PCM_HW_PARAM_CHANNELS - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL],
		             cfg->channels, cfg->channels);
		set_interval(&hw.intervals[SNDRV_PCM_HW_PARAM_RATE - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL],
		             cfg->rate, cfg->rate);
		ret = ioctl(fd, SNDRV_PCM_IOCTL_HW_PARAMS, &hw);
	}

	if (ret < 0) {
		log_err("SNDRV_PCM_IOCTL_HW_PARAMS failed: %s (errno %d)",
		        strerror(errno), errno);
		return -errno;
	}

	unsigned int period_frames = hw.intervals[SNDRV_PCM_HW_PARAM_PERIOD_SIZE - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL].min;
	unsigned int buffer_frames = hw.intervals[SNDRV_PCM_HW_PARAM_BUFFER_SIZE - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL].min;

	if (out_period_frames)
		*out_period_frames = period_frames;
	if (out_buffer_frames)
		*out_buffer_frames = buffer_frames;

	log_dbg(cfg, "HW Params negotiated: period_size=%u, buffer_size=%u", period_frames, buffer_frames);

	/* Configure SW parameters */
	memset(&sw, 0, sizeof(sw));
	sw.tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
	sw.period_step = 1;
	sw.avail_min = period_frames;
	sw.start_threshold = 1;
	sw.stop_threshold = buffer_frames;
	sw.silence_threshold = 0;
	sw.silence_size = 0;
	sw.boundary = buffer_frames;
	while (sw.boundary * 2 <= (snd_pcm_uframes_t)LONG_MAX - buffer_frames)
		sw.boundary *= 2;

	ret = ioctl(fd, SNDRV_PCM_IOCTL_SW_PARAMS, &sw);
	if (ret < 0) {
		log_err("SNDRV_PCM_IOCTL_SW_PARAMS failed: %s (errno %d)",
		        strerror(errno), errno);
		return -errno;
	}

	ret = ioctl(fd, SNDRV_PCM_IOCTL_PREPARE);
	if (ret < 0) {
		log_err("SNDRV_PCM_IOCTL_PREPARE failed: %s (errno %d)",
		        strerror(errno), errno);
		return -errno;
	}

	return 0;
}

/* -------------------------------------------------------------------------
 * WAV File Writing Helpers
 * ---------------------------------------------------------------------- */
static void wav_init_header(struct wav_header *hdr, unsigned int rate,
                            unsigned int channels, unsigned int bits_per_sample)
{
	memcpy(hdr->riff, "RIFF", 4);
	hdr->file_size = sizeof(struct wav_header) - 8;
	memcpy(hdr->wave, "WAVE", 4);
	memcpy(hdr->fmt, "fmt ", 4);
	hdr->fmt_size = 16;
	hdr->audio_format = 1; /* PCM */
	hdr->channels = channels;
	hdr->sample_rate = rate;
	hdr->bits_per_sample = bits_per_sample;
	hdr->block_align = channels * (bits_per_sample / 8);
	hdr->byte_rate = rate * hdr->block_align;
	memcpy(hdr->data, "data", 4);
	hdr->data_size = 0;
}

static void wav_finalize_header(FILE *fp, uint32_t data_bytes)
{
	if (!fp)
		return;

	uint32_t file_size = data_bytes + sizeof(struct wav_header) - 8;

	fseek(fp, 4, SEEK_SET);
	fwrite(&file_size, sizeof(uint32_t), 1, fp);

	fseek(fp, 40, SEEK_SET);
	fwrite(&data_bytes, sizeof(uint32_t), 1, fp);

	fflush(fp);
}

/* -------------------------------------------------------------------------
 * Audio Statistics Processing
 * ---------------------------------------------------------------------- */
static void update_stats(struct channel_stats *stats, const void *buf,
                         unsigned int frames, unsigned int channels, int format)
{
	if (format == SNDRV_PCM_FORMAT_S16_LE) {
		const int16_t *s = (const int16_t *)buf;
		for (unsigned int f = 0; f < frames; f++) {
			for (unsigned int c = 0; c < channels; c++) {
				int64_t v = *s++;
				stats[c].sum += v;
				stats[c].sum_sq += v * v;
				if (v < stats[c].min_val) stats[c].min_val = v;
				if (v > stats[c].max_val) stats[c].max_val = v;
				stats[c].sample_count++;
			}
		}
	} else if (format == SNDRV_PCM_FORMAT_S32_LE) {
		const int32_t *s = (const int32_t *)buf;
		for (unsigned int f = 0; f < frames; f++) {
			for (unsigned int c = 0; c < channels; c++) {
				int64_t v = *s++;
				stats[c].sum += v;
				stats[c].sum_sq += v * v;
				if (v < stats[c].min_val) stats[c].min_val = v;
				if (v > stats[c].max_val) stats[c].max_val = v;
				stats[c].sample_count++;
			}
		}
	}
}

static void print_channel_stats(const struct channel_stats *stats,
                                unsigned int channels, int format)
{
	double max_possible = (format == SNDRV_PCM_FORMAT_S16_LE) ? 32768.0 : 2147483648.0;

	for (unsigned int c = 0; c < channels; c++) {
		if (stats[c].sample_count == 0)
			continue;

		double n = (double)stats[c].sample_count;
		double mean = (double)stats[c].sum / n;
		double mean_sq = (double)stats[c].sum_sq / n;
		double variance = mean_sq - (mean * mean);
		double ac_rms = sqrt(variance > 0.0 ? variance : 0.0);
		double peak = fmax(fabs((double)stats[c].min_val), fabs((double)stats[c].max_val));

		double rms_dbfs = 20.0 * log10((ac_rms > 0.0 ? ac_rms : 1.0) / max_possible);
		double peak_dbfs = 20.0 * log10((peak > 0.0 ? peak : 1.0) / max_possible);

		log_info("  Channel %u: Min=%-6" PRId64 " Max=%-6" PRId64 " DC=%-8.1f RMS=%-8.1f (%.1f dBFS) Peak=%.1f (%.1f dBFS)",
		         c, stats[c].min_val, stats[c].max_val, mean, ac_rms, rms_dbfs, peak, peak_dbfs);
	}
}

/* -------------------------------------------------------------------------
 * Single Capture Cycle Execution
 * ---------------------------------------------------------------------- */
static int run_capture_cycle(const struct app_config *cfg, int cycle_idx, char *out_wav_path, size_t path_sz)
{
	char dev_path[64];
	snprintf(dev_path, sizeof(dev_path), PCM_DEV_FMT, cfg->card, cfg->device);

	if (cfg->out_file && cfg->cycles == 1) {
		snprintf(out_wav_path, path_sz, "%s", cfg->out_file);
	} else {
		time_t now = time(NULL);
		struct tm *tm = localtime(&now);
		snprintf(out_wav_path, path_sz, "%s/wov_%04d%02d%02d_%02d%02d%02d_cyc%03d.wav",
		         cfg->out_dir,
		         tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		         tm->tm_hour, tm->tm_min, tm->tm_sec,
		         cycle_idx);
	}

	log_state("Cycle %d: Opening capture device %s (rate=%u, ch=%u)",
	          cycle_idx, dev_path, cfg->rate, cfg->channels);

	int fd = open(dev_path, O_RDWR);
	if (fd < 0) {
		log_err("Failed to open %s: %s (errno %d)", dev_path, strerror(errno), errno);
		return -errno;
	}

	unsigned int actual_period_frames = 0;
	unsigned int buffer_frames = 0;
	int ret = pcm_configure(fd, cfg, &actual_period_frames, &buffer_frames);
	if (ret < 0) {
		close(fd);
		return ret;
	}

	FILE *wav_fp = fopen(out_wav_path, "wb");
	if (!wav_fp) {
		log_err("Failed to create WAV file %s: %s", out_wav_path, strerror(errno));
		close(fd);
		return -errno;
	}

	unsigned int bytes_per_sample = (cfg->format == SNDRV_PCM_FORMAT_S16_LE) ? 2 : 4;
	unsigned int frame_bytes = cfg->channels * bytes_per_sample;

	struct wav_header hdr;
	wav_init_header(&hdr, cfg->rate, cfg->channels, bytes_per_sample * 8);
	fwrite(&hdr, sizeof(hdr), 1, wav_fp);

	size_t period_bytes = actual_period_frames * frame_bytes;
	uint8_t *period_buf = malloc(period_bytes);
	if (!period_buf) {
		log_err("Failed to allocate capture buffer (%zu bytes)", period_bytes);
		fclose(wav_fp);
		close(fd);
		return -ENOMEM;
	}

	struct channel_stats *stats = calloc(cfg->channels, sizeof(struct channel_stats));
	for (unsigned int c = 0; c < cfg->channels; c++) {
		stats[c].min_val = INT64_MAX;
		stats[c].max_val = INT64_MIN;
	}

	uint64_t target_frames = (cfg->duration_s > 0) ? (uint64_t)(cfg->duration_s * cfg->rate) : UINT64_MAX;
	uint64_t total_frames_captured = 0;
	uint32_t total_bytes_written = 0;

	log_state("Cycle %d: Capturing audio -> %s", cycle_idx, out_wav_path);

	ret = ioctl(fd, SNDRV_PCM_IOCTL_START);
	if (ret < 0 && errno != EBADFD) {
		/* Start may auto-trigger on first read, but explicit start is fine */
		log_dbg(cfg, "SNDRV_PCM_IOCTL_START info: %s", strerror(errno));
	}

	while (!g_stop && total_frames_captured < target_frames) {
		unsigned int frames_to_read = actual_period_frames;
		if (target_frames != UINT64_MAX && (total_frames_captured + frames_to_read) > target_frames)
			frames_to_read = target_frames - total_frames_captured;

		struct snd_xferi xferi;
		xferi.result = 0;
		xferi.buf = period_buf;
		xferi.frames = frames_to_read;

		ret = ioctl(fd, SNDRV_PCM_IOCTL_READI_FRAMES, &xferi);
		if (ret < 0) {
			if (errno == EPIPE) {
				log_dbg(cfg, "PCM XRUN (overrun) detected, recovering...");
				ioctl(fd, SNDRV_PCM_IOCTL_PREPARE);
				ioctl(fd, SNDRV_PCM_IOCTL_START);
				continue;
			}
			log_err("READI_FRAMES error: %s (errno %d)", strerror(errno), errno);
			break;
		}

		snd_pcm_sframes_t frames_read = xferi.result;
		if (frames_read > 0) {
			size_t bytes = frames_read * frame_bytes;
			fwrite(period_buf, 1, bytes, wav_fp);
			total_bytes_written += bytes;
			total_frames_captured += frames_read;

			update_stats(stats, period_buf, frames_read, cfg->channels, cfg->format);
		}
	}

	/* Stop capture */
	ioctl(fd, SNDRV_PCM_IOCTL_DROP);
	close(fd);

	/* Finalize WAV file header */
	wav_finalize_header(wav_fp, total_bytes_written);
	fclose(wav_fp);
	free(period_buf);

	double duration_actual = (double)total_frames_captured / (double)cfg->rate;
	log_state("Cycle %d: Captured %" PRIu64 " frames (%.2fs, %u bytes) -> %s",
	          cycle_idx, total_frames_captured, duration_actual, total_bytes_written, out_wav_path);

	print_channel_stats(stats, cfg->channels, cfg->format);
	free(stats);

	return 0;
}

/* -------------------------------------------------------------------------
 * Usage & Main
 * ---------------------------------------------------------------------- */
static void print_usage(const char *prog)
{
	fprintf(stdout,
		"Usage: %s [options]\n\n"
		"WOV Regular PCM Capture Tool for SOF IPC4 Audio Pipelines.\n\n"
		"Options:\n"
		"  -c CARD      Sound card number (default: %u)\n"
		"  -d DEV       PCM device number (default: %u)\n"
		"  -r RATE      Sample rate in Hz (default: %u)\n"
		"  -C CHANS     Number of channels (default: %u)\n"
		"  -f FORMAT    Sample format: S16_LE (default) or S32_LE\n"
		"  -t DURATION  Capture duration in seconds (default: %.1f, 0 = unlimited)\n"
		"  -n CYCLES    Number of capture cycles (default: %d, 0 = unlimited)\n"
		"  -w DELAY     Delay between cycles in seconds (default: 0.5)\n"
		"  -o DIR       Output directory for WAV files (default: %s)\n"
		"  -F FILE      Explicit output WAV file path\n"
		"  -v           Verbose output (print extra debug info)\n"
		"  -h           Show this help message\n\n"
		"Examples:\n"
		"  %s -c 0 -d 11 -r 16000 -C 2 -t 3.0 -o /tmp\n"
		"  %s -c 0 -d 11 -n 5 -t 2.0 -w 1.0\n"
		"  %s -F /tmp/test_dmic_16k.wav -t 5.0\n",
		prog,
		DEFAULT_CARD, DEFAULT_DEVICE, DEFAULT_RATE, DEFAULT_CHANNELS,
		DEFAULT_DURATION_S, DEFAULT_CYCLES, DEFAULT_OUT_DIR,
		prog, prog, prog);
}

int main(int argc, char **argv)
{
	struct app_config cfg = {
		.card          = DEFAULT_CARD,
		.device        = DEFAULT_DEVICE,
		.channels      = DEFAULT_CHANNELS,
		.rate          = DEFAULT_RATE,
		.format        = DEFAULT_FORMAT,
		.period_frames = DEFAULT_PERIOD_FRAMES,
		.periods       = DEFAULT_PERIODS,
		.duration_s    = DEFAULT_DURATION_S,
		.cycles        = DEFAULT_CYCLES,
		.cycle_delay_s = 0.5,
		.out_dir       = DEFAULT_OUT_DIR,
		.out_file      = NULL,
		.verbose       = 0,
	};

	static struct option long_opts[] = {
		{"card",      required_argument, 0, 'c'},
		{"device",    required_argument, 0, 'd'},
		{"rate",      required_argument, 0, 'r'},
		{"channels",  required_argument, 0, 'C'},
		{"format",    required_argument, 0, 'f'},
		{"duration",  required_argument, 0, 't'},
		{"cycles",    required_argument, 0, 'n'},
		{"delay",     required_argument, 0, 'w'},
		{"out-dir",   required_argument, 0, 'o'},
		{"out-file",  required_argument, 0, 'F'},
		{"verbose",   no_argument,       0, 'v'},
		{"help",      no_argument,       0, 'h'},
		{0, 0, 0, 0}
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "c:d:r:C:f:t:n:w:o:F:vh", long_opts, NULL)) != -1) {
		switch (opt) {
		case 'c': cfg.card = atoi(optarg); break;
		case 'd': cfg.device = atoi(optarg); break;
		case 'r': cfg.rate = atoi(optarg); break;
		case 'C': cfg.channels = atoi(optarg); break;
		case 'f':
			if (strcasecmp(optarg, "S16_LE") == 0 || strcasecmp(optarg, "S16") == 0)
				cfg.format = SNDRV_PCM_FORMAT_S16_LE;
			else if (strcasecmp(optarg, "S32_LE") == 0 || strcasecmp(optarg, "S32") == 0)
				cfg.format = SNDRV_PCM_FORMAT_S32_LE;
			else {
				fprintf(stderr, "Unsupported format: %s (use S16_LE or S32_LE)\n", optarg);
				return 1;
			}
			break;
		case 't': cfg.duration_s = atof(optarg); break;
		case 'n': cfg.cycles = atoi(optarg); break;
		case 'w': cfg.cycle_delay_s = atof(optarg); break;
		case 'o': cfg.out_dir = optarg; break;
		case 'F': cfg.out_file = optarg; break;
		case 'v': cfg.verbose = 1; break;
		case 'h': print_usage(argv[0]); return 0;
		default:  print_usage(argv[0]); return 1;
		}
	}

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	log_info("Starting WOV PCM Capture App (Card=%u, Dev=%u, Rate=%u, Ch=%u, Format=%s)",
	         cfg.card, cfg.device, cfg.rate, cfg.channels,
	         cfg.format == SNDRV_PCM_FORMAT_S16_LE ? "S16_LE" : "S32_LE");

	int cycle = 1;
	char wav_path[512];

	while (!g_stop) {
		int ret = run_capture_cycle(&cfg, cycle, wav_path, sizeof(wav_path));
		if (ret < 0) {
			log_err("Cycle %d failed with error %d", cycle, ret);
			return 1;
		}

		if (cfg.cycles > 0 && cycle >= cfg.cycles)
			break;

		cycle++;

		if (cfg.cycle_delay_s > 0.0 && !g_stop) {
			log_dbg(&cfg, "Waiting %.2fs before next cycle...", cfg.cycle_delay_s);
			usleep((useconds_t)(cfg.cycle_delay_s * 1000000.0));
		}
	}

	log_info("WOV capture completed successfully.");
	return 0;
}
