/* SPDX-License-Identifier: BSD-3-Clause
 *
 * WOV Blocking Read Utility using tinyalsa.
 *
 * Demonstrates Wake-on-Voice gated capture using tinyalsa's PCM_MMAP and
 * PCM_NOIRQ (SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP) to perform blocking
 * capture without hitting the kernel's 500ms wait timeout.
 *
 * Usage: wov_blocking_read_tinyalsa [hw:CARD,DEV] [KCONTROL_NAME]
 * Example: wov_blocking_read_tinyalsa hw:0,12 wovdebug_111
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tinyalsa/asoundlib.h>

static double now_s(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
	unsigned int card = 0;
	unsigned int device = 12;
	const char *ctl = NULL;
	const char *dev_str = argc > 1 ? argv[1] : "hw:0,12";

	if (argc > 2)
		ctl = argv[2];

	/* Parse card and device from "hw:C,D" or "C,D" */
	if (sscanf(dev_str, "hw:%u,%u", &card, &device) != 2) {
		if (sscanf(dev_str, "%u,%u", &card, &device) != 2) {
			/* Fallback to default */
			card = 0;
			device = 12;
		}
	}

	struct pcm_config config;
	memset(&config, 0, sizeof(config));
	config.channels = 1;
	config.rate = 16000;
	config.period_size = 1024;
	config.period_count = 4;
	config.format = PCM_FORMAT_S16_LE;
	config.start_threshold = 1;
	config.stop_threshold = 0;
	config.silence_threshold = 0;

	/* Open with PCM_IN, PCM_MMAP, and PCM_NOIRQ */
	unsigned int flags = PCM_IN | PCM_MMAP | PCM_NOIRQ;
	struct pcm *pcm = pcm_open(card, device, flags, &config);
	if (!pcm || !pcm_is_ready(pcm)) {
		fprintf(stderr, "pcm_open(card=%u, dev=%u) failed: %s\n",
		        card, device, pcm ? pcm_get_error(pcm) : "unknown error");
		if (pcm)
			pcm_close(pcm);
		return 1;
	}

	printf("period_size=%u buffer_size=%u rate=%u (tinyalsa)\n",
	       config.period_size, pcm_get_buffer_size(pcm), config.rate);

	/* Arm control if specified */
	if (ctl) {
		char cmd[128];
		snprintf(cmd, sizeof(cmd),
		         "tinymix -D %u set %s 1 >/dev/null 2>&1 || amixer -c %u cset name=%s 1 >/dev/null 2>&1",
		         card, ctl, card, ctl);
		system(cmd);
	}

	short buf[4000];
	double t0 = now_s();
	printf("t=%.3f calling blocking read for %zu frames...\n", 0.0,
	       sizeof(buf) / sizeof(buf[0]));
	fflush(stdout);

	int ret = pcm_readi(pcm, buf, sizeof(buf) / sizeof(buf[0]));
	double t1 = now_s();

	if (ret < 0) {
		fprintf(stderr, "t=%.3f read error: %s (code %d)\n",
		        t1 - t0, pcm_get_error(pcm), ret);
		pcm_close(pcm);
		return 1;
	}

	printf("t=%.3f read returned %d frames after %.3fs\n",
	       t1 - t0, ret, t1 - t0);

	pcm_stop(pcm);
	pcm_close(pcm);
	return 0;
}
