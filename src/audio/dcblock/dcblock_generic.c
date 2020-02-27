#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/dcblock/dcblock.h>

/**
 *
 * Genereric processing function. Input is 32 bits.
 *
 */
static int32_t dcblock_generic(struct dcblock_state *dcblock, int32_t x) {
  int32_t x_prev = dcblock->x_prev;
  int32_t y_prev = dcblock->y_prev;
  int64_t R = dcblock->R_coeff;

  int64_t out = x - x_prev + Q_SHIFT_RND(R * y_prev, 62, 31);

	y_prev = sat_int32(out);
  x_prev = x;

  dcblock->x_prev = x_prev;
	dcblock->y_prev = y_prev;

	return sat_int32(out);
}

#if CONFIG_FORMAT_S16LE
static void dcblock_s16_default(const struct comp_dev *dev,
									const struct audio_stream *source,
									const struct audio_stream *sink,
									uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
  struct dcblock_state *filter;
	int16_t *x;
	int16_t *y;
	int32_t tmp;
	int idx;
	int ch;
	int i;
	int nch = source->channels;

	for(ch = 0; ch < nch; ch++) {
    filter = &cd->dcblock[ch];
		idx = ch;
		for(i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s16(source, idx);
			y = audio_stream_read_frag_s16(sink, idx);
			tmp = dcblock_generic(filter, *x << 16);
			*y = sat_int16(Q_SHIFT_RND(tmp, 31, 15));
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void dcblock_s24_default(const struct comp_dev *dev,
									const struct audio_stream *source,
									const struct audio_stream *sink,
									uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
  struct dcblock_state *filter;
	int32_t *x;
	int32_t *y;
  int32_t tmp;
	int idx;
	int ch;
	int i;
	int nch = source->channels;

	for(ch = 0; ch < nch; ch++) {
    filter = &cd->dcblock[ch];
		idx = ch;
		for(i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s32(source, idx);
			y = audio_stream_read_frag_s32(sink, idx);
      tmp = dcblock_generic(filter, *x << 8);
			*y = sat_int24(Q_SHIFT_RND(tmp, 31, 23));
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void dcblock_s32_default(const struct comp_dev *dev,
									const struct audio_stream *source,
									const struct audio_stream *sink,
                  uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
  struct dcblock_state *filter;
	int32_t *x;
	int32_t *y;
	int idx;
	int ch;
	int i;
	int nch = source->channels;

	for(ch = 0; ch < nch; ch++) {
    filter = &cd->dcblock[ch];
		idx = ch;
		for(i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s32(source, idx);
			y = audio_stream_read_frag_s32(sink, idx);
			*y = dcblock_generic(filter, *x);
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S32LE */

const struct dcblock_func_map dcblock_fnmap[] = {
  /* { SOURCE_FORMAT  , SINK_FORMAT  , PROCESSING FUNCTION } */
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, dcblock_s16_default },
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, dcblock_s24_default },
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, dcblock_s32_default },
#endif /* CONFIG_FORMAT_S32LE */
};

const size_t dcblock_fncount = ARRAY_SIZE(dcblock_fnmap);
