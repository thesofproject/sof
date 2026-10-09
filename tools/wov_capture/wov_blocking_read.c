// SPDX-License-Identifier: BSD-3-Clause
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <alsa/asoundlib.h>

struct wav_header {
	char     riff[4];
	uint32_t file_size;
	char     wave[4];
	char     fmt[4];
	uint32_t fmt_size;
	uint16_t audio_format;
	uint16_t channels;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t block_align;
	uint16_t bits_per_sample;
	char     data[4];
	uint32_t data_size;
} __attribute__((packed));

static double now_s(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int get_active_slot(int card)
{
	snd_ctl_t *ctl_handle;
	char ctl_name[32];
	snprintf(ctl_name, sizeof(ctl_name), "hw:%d", card);
	if (snd_ctl_open(&ctl_handle, ctl_name, 0) < 0)
		return -1;

	snd_ctl_elem_id_t *id;
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_id_set_name(id, "wov_active_slot");

	snd_ctl_elem_value_t *val;
	snd_ctl_elem_value_alloca(&val);
	snd_ctl_elem_value_set_id(val, id);

	int slot = -1;
	for (int retry = 0; retry < 30; retry++) {
		if (snd_ctl_elem_read(ctl_handle, val) >= 0) {
			slot = snd_ctl_elem_value_get_enumerated(val, 0);
			if (slot > 0)
				break;
		}
		usleep(10000);
	}
	snd_ctl_close(ctl_handle);
	return slot;
}

int main(int argc, char **argv)
{
	const char *device = argc > 1 ? argv[1] : "hw:0,12";
	const char *ctl = argc > 2 ? argv[2] : NULL;
	unsigned int delay_sec = argc > 3 ? atoi(argv[3]) : 0;
	const char *out_wav = (argc > 4 && strlen(argv[4]) > 0 && strcmp(argv[4], "none") != 0) ? argv[4] : NULL;
	unsigned int total_frames = argc > 5 ? atoi(argv[5]) : 4000;
	unsigned int rate = 16000;
	snd_pcm_t *pcm;
	snd_pcm_hw_params_t *hw;
	int err;

	int card = 0;
	if (strncmp(device, "hw:", 3) == 0)
		card = atoi(device + 3);

	int triggered_slot = -1;
	int expected_slot = 0;
	if (ctl) {
		size_t len = strlen(ctl);
		if (len > 0 && ctl[len - 1] >= '1' && ctl[len - 1] <= '9')
			expected_slot = ctl[len - 1] - '0';
	}

	err = snd_pcm_open(&pcm, device, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
	if (err < 0) {
		fprintf(stderr, "open failed: %s\n", snd_strerror(err));
		return 1;
	}

	snd_pcm_hw_params_alloca(&hw);
	snd_pcm_hw_params_any(pcm, hw);
	snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE);
	snd_pcm_hw_params_set_channels(pcm, hw, 1);
	snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0);

	err = snd_pcm_hw_params(pcm, hw);
	if (err < 0) {
		fprintf(stderr, "hw_params failed: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}

	snd_pcm_uframes_t period_size = 0, buffer_size = 0;
	snd_pcm_hw_params_get_period_size(hw, &period_size, NULL);
	snd_pcm_hw_params_get_buffer_size(hw, &buffer_size);
	printf("period_size=%lu buffer_size=%lu rate=%u\n",
	       (unsigned long)period_size, (unsigned long)buffer_size, rate);

	/* Set software params: avail_min = 160 frames (10ms) so poll() wakes on any drained audio */
	snd_pcm_sw_params_t *sw;
	snd_pcm_sw_params_alloca(&sw);
	snd_pcm_sw_params_current(pcm, sw);
	snd_pcm_sw_params_set_avail_min(pcm, sw, 160);
	err = snd_pcm_sw_params(pcm, sw);
	if (err < 0) {
		fprintf(stderr, "sw_params failed: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}

	err = snd_pcm_prepare(pcm);
	if (err < 0) {
		fprintf(stderr, "prepare failed: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}

	err = snd_pcm_start(pcm);
	if (err < 0) {
		fprintf(stderr, "start failed: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}

	if (delay_sec > 0) {
		printf("Waiting %u seconds idle in stream for D0i3 entry...\n", delay_sec);
		fflush(stdout);
		sleep(delay_sec);
	}

	if (ctl) {
		char cmd[128];
		snprintf(cmd, sizeof(cmd), "amixer -c %d cset name=%s 1", card, ctl);
		int rc = system(cmd);
		if (rc != 0) {
			fprintf(stderr, "warning: '%s' returned %d, retrying after 150ms...\n", cmd, rc);
			usleep(150000);
			rc = system(cmd);
			if (rc != 0)
				fprintf(stderr, "error: '%s' retry failed with code %d\n", cmd, rc);
		}
	}

	FILE *wav_fp = NULL;
	if (out_wav) {
		wav_fp = fopen(out_wav, "wb");
		if (wav_fp) {
			struct wav_header hdr;
			memcpy(hdr.riff, "RIFF", 4);
			hdr.file_size = 0;
			memcpy(hdr.wave, "WAVE", 4);
			memcpy(hdr.fmt, "fmt ", 4);
			hdr.fmt_size = 16;
			hdr.audio_format = 1;
			hdr.channels = 1;
			hdr.sample_rate = rate;
			hdr.byte_rate = rate * 2;
			hdr.block_align = 2;
			hdr.bits_per_sample = 16;
			memcpy(hdr.data, "data", 4);
			hdr.data_size = 0;
			fwrite(&hdr, sizeof(hdr), 1, wav_fp);
		}
	}

	short buf[1024];
	unsigned int frames_captured = 0;
	double t0 = now_s();
	printf("t=%.3f waiting for keyword trigger / audio (%u frames)...\n", 0.0, total_frames);
	fflush(stdout);

	while (frames_captured < total_frames) {
		snd_pcm_uframes_t to_read = sizeof(buf) / sizeof(buf[0]);
		if (to_read > total_frames - frames_captured)
			to_read = total_frames - frames_captured;

		snd_pcm_sframes_t n = snd_pcm_readi(pcm, buf, to_read);
		if (n < 0) {
			if (n == -EAGAIN) {
				err = snd_pcm_wait(pcm, 15000);
				if (err == -ESTRPIPE) {
					snd_pcm_recover(pcm, err, 0);
					continue;
				}
				if (err <= 0) {
					fprintf(stderr, "wait error or timeout: %d\n", err);
					break;
				}
				continue;
			}
			if (n == -EPIPE || n == -ESTRPIPE) {
				snd_pcm_recover(pcm, n, 0);
				continue;
			}
			fprintf(stderr, "read error: %s\n", snd_strerror((int)n));
			break;
		}
		if (n > 0) {
			if (wav_fp)
				fwrite(buf, sizeof(short), n, wav_fp);
			frames_captured += n;
		}
	}

	for (int retry = 0; retry < 25; retry++) {
		int s = get_active_slot(card);
		if (s > 0) {
			triggered_slot = s;
			break;
		}
		usleep(20000);
	}
	printf("t=%.3f triggered active_slot=%d (expected=%d)\n",
	       now_s() - t0, triggered_slot > 0 ? triggered_slot : 0, expected_slot);
	fflush(stdout);

	double t1 = now_s();
	printf("t=%.3f capture finished: %u frames after %.3fs\n", t1 - t0, frames_captured, t1 - t0);

	if (wav_fp) {
		uint32_t data_bytes = frames_captured * sizeof(short);
		uint32_t file_size = data_bytes + sizeof(struct wav_header) - 8;
		fseek(wav_fp, 4, SEEK_SET);
		fwrite(&file_size, sizeof(uint32_t), 1, wav_fp);
		fseek(wav_fp, 40, SEEK_SET);
		fwrite(&data_bytes, sizeof(uint32_t), 1, wav_fp);
		fclose(wav_fp);
	}

	if (ctl) {
		char cmd[128];
		snprintf(cmd, sizeof(cmd), "amixer -c 0 cset name=%s 0 >/dev/null 2>&1", ctl);
		system(cmd);
	}

	snd_pcm_drop(pcm);
	/* Allow STOP notification to settle in ALSA control cache before close */
	usleep(150000);
	snd_pcm_close(pcm);

	int ok = (frames_captured >= total_frames);
	if (expected_slot > 0 && triggered_slot != expected_slot) {
		fprintf(stderr, "error: triggered slot %d does not match expected slot %d\n",
			triggered_slot, expected_slot);
		ok = 0;
	}
	return ok ? 0 : 1;
}
