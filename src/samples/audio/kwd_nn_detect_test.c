// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2021 NXP
//
// Author: Cristina Feies <cristina.ilie@nxp.com>
// Author: Viorel Suman <viorel.suman@nxp.com>

#include <sof/samples/audio/kwd_nn_detect_test.h>
#include <sof/samples/audio/kwd_nn/kwd_nn_preprocess.h>
#include <sof/samples/audio/kwd_nn/kwd_nn_process.h>
#include <sof/samples/audio/detect_test.h>
#include <sof/audio/component.h>
#include <sof/lib/wait.h>
#include <stdint.h>

static __aligned(64) uint8_t preprocessed_data[KWD_NN_CONFIG_PREPROCESSED_SIZE];
static __aligned(8) uint8_t confidences[KWD_NN_CONFIDENCES_SIZE];

static int kwd_nn_detect_postprocess(uint8_t confidences[KWD_NN_CONFIDENCES_SIZE])
{
	int i;
	uint8_t max_confidence = confidences[0];
	int max_idx = 0;

	for (i = 1; i < KWD_NN_CONFIDENCES_SIZE; i++) {
		if (max_confidence < confidences[i]) {
			max_confidence = confidences[i];
			max_idx = i;
		}
	}
	if (max_confidence < KWD_NN_MIN_ACCEPTABLE_CONFIDENCE)
		max_idx = KWD_NN_UNKNOWN;
	return max_idx;
}

void kwd_nn_detect_test(struct comp_dev *dev,
			const struct audio_stream *source,
			uint32_t frames)
{
	void *src;
	uint32_t count = frames; /**< Assuming single channel */
	uint32_t sample;
	uint32_t one_sec_samples = KWD_NN_KEY_LEN;
	uint32_t half_sec_samples = one_sec_samples / 2;

	/* perform detection within current period */
	for (sample = 0; sample < count && !test_keyword_get_detected(dev); ++sample) {
		src = (test_keyword_get_sample_valid_bytes(dev) * 8 == 16U) ?
			audio_stream_read_frag_s16(source, sample) :
			audio_stream_read_frag_s32(source, sample);
		if (test_keyword_get_input_size(dev) < KWD_NN_IN_BUFF_SIZE) {
			if (test_keyword_get_sample_valid_bytes(dev) == 16U)
				test_keyword_set_input_elem(dev,
							    test_keyword_get_input_size(dev),
							    *((int16_t *)src));
			else
				test_keyword_set_input_elem(dev,
							    test_keyword_get_input_size(dev),
							    *((int32_t *)src));
			test_keyword_set_input_size(dev, test_keyword_get_input_size(dev) + 1);
		}
		if (test_keyword_get_input_size(dev) > one_sec_samples) {
			uint64_t time_start;
			uint64_t time_stop;
			struct timer *timer = timer_get();
			int result;
			int i, j;

			comp_dbg(dev, "Drain values (0-3): 0x%x, 0x%x, 0x%x, 0x%x\n",
				 test_keyword_get_input_byte(dev, 0),
				 test_keyword_get_input_byte(dev, 1),
				 test_keyword_get_input_byte(dev, 2),
				 test_keyword_get_input_byte(dev, 3)
			);
			comp_dbg(dev, "Drain values (4-7): 0x%x, 0x%x, 0x%x, 0x%x\n",
				 test_keyword_get_input_byte(dev, 4),
				 test_keyword_get_input_byte(dev, 5),
				 test_keyword_get_input_byte(dev, 6),
				 test_keyword_get_input_byte(dev, 7)
			);
			time_start = platform_timer_get(timer);
			kwd_nn_preprocess_1s(test_keyword_get_input(dev), preprocessed_data);
			kwd_nn_process_data(preprocessed_data, confidences);
			result = kwd_nn_detect_postprocess(confidences);
			time_stop = platform_timer_get(timer);
			comp_dbg(dev,
				 "KWD: kwd_nn_detect_test_copy() inference done in %u ms",
				 (unsigned int)((time_stop - time_start)
				 / clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1)));
			switch (result) {
			case KWD_NN_YES_KEYWORD:
			case KWD_NN_NO_KEYWORD:
				if (result == KWD_NN_NO_KEYWORD)
					comp_info(dev,
						  "kwd_nn_detect_test_copy(): keyword NO detected confidence %d",
						  confidences[3]);
				else
					comp_info(dev,
						  "kwd_nn_detect_test_copy(): keyword YES detected confidences %d",
						  confidences[2]);
				/* The algorithm shall use cd->drain_req
				 * to specify its draining size request.
				 * Zero value means default config value
				 * will be used.
				 */
				test_keyword_set_drain_req(dev, 0);
				detect_test_notify(dev);
				test_keyword_set_detected(dev, 1);
				break;
			default:
				if (result == KWD_NN_SILENCE)
					comp_dbg(dev,
						 "detect_test_copy(): SILENCE detected conf %d",
						 confidences[0]);
				else if (result == KWD_NN_UNKNOWN)
					comp_dbg(dev,
						 "detect_test_copy(): UNKNOWN detected conf %d",
						 confidences[1]);
				/* now shift input buffer half second left */
				for (i = half_sec_samples, j = 0;
						i < test_keyword_get_input_size(dev); i++, j++)
					/* input[j] = input[i] */
					test_keyword_set_input_elem(dev, j,
								    test_keyword_get_input_elem
								    (dev, i));
				test_keyword_set_input_size(dev,
							    test_keyword_get_input_size(dev) -
							    half_sec_samples);
				break;
			}
		}
	}
}
