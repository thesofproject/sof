// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <ipc/topology.h>
#include <sof/audio/tdfb/tdfb_comp.h>
#include <rtos/alloc.h>
#include <sof/math/iir_df2t.h>
#include <sof/math/trig.h>
#include <sof/math/sqrt.h>
#include <user/eq.h>
#include <stdint.h>

/* Generic definitions */
#define COEF_DEG_TO_RAD		Q_CONVERT_FLOAT(0.017453, 15)	/* Q1.15 */
#define COEF_RAD_TO_DEG		Q_CONVERT_FLOAT(57.296, 9)	/* Q6.9 */
#define PI_Q12			Q_CONVERT_FLOAT(3.1416, 12)
#define PIMUL2_Q12		Q_CONVERT_FLOAT(6.2832, 12)
#define PIDIV2_Q12		Q_CONVERT_FLOAT(1.5708, 12)

/* Sound levels filtering related, these form a primitive voice activity
 * detector. Sound levels below ambient estimate times threshold (kind of dB offset)
 * are not scanned for sound direction.
 */
#define SLOW_LEVEL_SHIFT	12
#define FAST_LEVEL_SHIFT	1
#define POWER_THRESHOLD		Q_CONVERT_FLOAT(15.849, 10)	/* 12 dB, value is 10^(dB/10) */

/* Iteration parameters, smaller step and larger iterations is more accurate but
 * consumes more cycles.
 */
#define AZ_STEP			Q_CONVERT_FLOAT(0.6, 12)	/* Radians as Q4.12 (~34 deg) */
#define AZ_ITERATIONS		8				/* loops in min err search */
#define SOURCE_DISTANCE		Q_CONVERT_FLOAT(3.0, 12)	/* source distance in m Q4.12 */

/* Sound direction angle filtering */
#define SLOW_AZ_C1		Q_CONVERT_FLOAT(0.02, 15)
#define SLOW_AZ_C2		Q_CONVERT_FLOAT(0.98, 15)

/* Threshold for notifying user space, no more often than every 200 ms */
#define CONTROL_UPDATE_MIN_TIME	Q_CONVERT_FLOAT(0.2, 16)

/* Emphasis filters for sound direction (IIR). These coefficients were
 * created with the script tools/tune/tdfb/example_direction_emphasis.m
 * from output files tdfb_iir_emphasis_48k.h and tdfb_iir_emphasis_16k.h.
 */

uint32_t iir_emphasis_48k[20] = {
	0x00000002, 0x00000002, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0xc8cf47b5, 0x7689916a,
	0x1dc95968, 0xc46d4d30, 0x1dc95968, 0x00000000,
	0x00004000, 0xe16f20ea, 0x51e57f66, 0x01966267,
	0x032cc4ce, 0x01966267, 0xfffffffe, 0x00004222
};

uint32_t iir_emphasis_16k[20] = {
	0x00000002, 0x00000002, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0xd6f418ae, 0x63e7b85c,
	0x19ae069b, 0xcca3f2ca, 0x19ae069b, 0x00000000,
	0x00004000, 0xf504f334, 0x00000000, 0x09651419,
	0x12ca2831, 0x09651419, 0xfffffffe, 0x0000414c
};

/**
 * \brief This function copies data to sound direction estimation
 *
 * Function to copy data from component source to cross correlation buffer. The copy
 * operation includes emphasis filtering. The copy is skipped if tracking is not
 * enabled.
 *
 * \param[in,out]	cd		Component data
 * \param[in]		ch_count	Number of channels in audio stream
 * \param[in,out]	ch		Current channel for emphasis, incremented
 *					and reset to zero after channel_count -1.
 * \param[in]		x		Input PCM sample
 */
void tdfb_direction_copy_emphasis(struct tdfb_comp_data *cd, int ch_count, int *ch, int32_t x)
{
	int32_t y;

	if (!cd->direction_updates)
		return;

	y = iir_df2t(&cd->direction.emphasis[*ch], x);
	*cd->direction.wp = sat_int16(Q_SHIFT_RND(y, 31, 18)); /* 18 dB boost after high-pass */
	cd->direction.wp++;
	tdfb_cinc_s16(&cd->direction.wp, cd->direction.d_end, cd->direction.d_size);
	(*ch)++;
	if (*ch == ch_count)
		*ch = 0;
}

/* Use function y = sqrt_int16(x), where x is Q4.12 unsigned, y is Q4.12
 * TODO: Add scaling of input to Q4.12 with a suitable N^2 or (1/N)^2 value
 * to increase sqrt() computation precision and avoid MIN() function to
 * saturate for large distances (over 4m). The current virtual source
 * distance 3m allows max 1m size array for this to work correctly without
 * hitting the saturation.
 */
static inline int16_t tdfb_mic_distance_sqrt(int32_t x)
{
	int32_t xs;

	xs = Q_SHIFT_RND(x, 24, 12);
	xs = MIN(xs, UINT16_MAX);
	return sqrt_int16((uint16_t)xs);
}

static int16_t max_mic_distance(struct tdfb_comp_data *cd)
{
	int16_t d;
	int16_t dx;
	int16_t dy;
	int16_t dz;
	int32_t d2;
	int32_t d2_max = 0;
	int i;
	int j;

	/* Max lag based on largest array dimension. Microphone coordinates are Q4.12 meters */
	for (i = 0; i < cd->config->num_mic_locations; i++) {
		for (j = 0; i < cd->config->num_mic_locations; i++) {
			if (j == i)
				continue;

			dx = cd->mic_locations[i].x - cd->mic_locations[j].x;
			dy = cd->mic_locations[i].y - cd->mic_locations[j].y;
			dz = cd->mic_locations[i].z - cd->mic_locations[j].z;

			/* d2 is Q8.24 meters */
			d2 = dx * dx + dy * dy + dz * dz;
			if (d2 > d2_max)
				d2_max = d2;
		}
	}

	/* Squared distance is Q8.24, return Q4.12 meters */
	d = tdfb_mic_distance_sqrt(d2_max);
	return d;
}

static bool line_array_mode_check(struct tdfb_comp_data *cd)
{
	int32_t px, py, pz;
	int16_t a, b, c, d, e, f;
	int i;
	int num_mic_locations = cd->config->num_mic_locations;

	if (num_mic_locations == 2)
		return true;

	/* Cross product of vectors AB and AC is (0, 0, 0) if they are co-linear.
	 * Form vector AB(a,b,c) from x(i+1) - x(i), y(i+1) - y(i), z(i+1) - z(i)
	 * Form vector AC(d,e,f) from x(i+2) - x(i), y(i+2) - y(1), z(i+2) - z(i)
	 */
	for (i = 0; i < num_mic_locations - 3; i++) {
		a = cd->mic_locations[i + 1].x - cd->mic_locations[i].x;
		b = cd->mic_locations[i + 1].y - cd->mic_locations[i].y;
		c = cd->mic_locations[i + 1].z - cd->mic_locations[i].z;
		d = cd->mic_locations[i + 2].x - cd->mic_locations[i].x;
		e = cd->mic_locations[i + 2].y - cd->mic_locations[i].y;
		f = cd->mic_locations[i + 2].z - cd->mic_locations[i].z;
		cross_product_s16(&px, &py, &pz, a, b, c, d, e, f);

		/* Allow small room for rounding error with |x| > 1 in check */
		if (ABS(px) > 1 || ABS(py) > 1 || ABS(pz) > 1)
			return false;
	}

	return true;
}

int tdfb_direction_init(struct tdfb_comp_data *cd, int32_t fs, int ch_count)
{
	struct sof_eq_iir_header_df2t *filt;
	int64_t *delay;
	int32_t d_max;
	int32_t t_max;
	size_t size;
	int n;
	int i;

	/* Select emphasis response per sample rate */
	switch (fs) {
	case 16000:
		filt = (struct sof_eq_iir_header_df2t *)iir_emphasis_16k;
		break;
	case 48000:
		filt = (struct sof_eq_iir_header_df2t *)iir_emphasis_48k;
		break;
	default:
		return -EINVAL;
	}

	/* Allocate delay lines for IIR filters and initialize them */
	size = ch_count * iir_delay_size_df2t(filt);
	delay = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, size);
	if (!delay)
		return -ENOMEM;

	cd->direction.df2t_delay = delay;
	for (i = 0; i < ch_count; i++) {
		iir_init_coef_df2t(&cd->direction.emphasis[i], filt);
		iir_init_delay_df2t(&cd->direction.emphasis[i], &delay);
	}

	/* Unit delay length in Q1.31 seconds */
	cd->direction.unit_delay = Q_SHIFT_LEFT(1LL, 0, 31) / fs;

	/* Get max possible mic-mic distance, then from max distance to max time t = d_max / v,
	 * t is Q1.15, d is Q4.12, v is Q9.0
	 */
	d_max = max_mic_distance(cd);
	t_max = Q_SHIFT_LEFT(d_max, 12, 15) / SPEED_OF_SOUND;

	/* Calculate max lag to search and allocate delay line for cross correlation
	 * compute. Add one to make sure max possible lag is in search window.
	 */
	cd->direction.max_lag = Q_MULTSR_32X32((int64_t)fs, t_max, 0, 15, 0) + 1;
	n = (cd->max_frames + (2 * cd->direction.max_lag + 1)) * ch_count;
	cd->direction.d_size =  n * sizeof(int16_t);
	cd->direction.d = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, cd->direction.d_size);
	if (!cd->direction.d)
		goto err_free_iir;

	/* Set needed pointers to xcorr delay line, advance write pointer by max_lag to keep read
	 * always behind write
	 */
	cd->direction.d_end = cd->direction.d + n;
	cd->direction.rp = cd->direction.d;
	cd->direction.wp = cd->direction.d + ch_count * (cd->direction.max_lag + 1);

	/* xcorr result is temporary but too large for stack so it is allocated here */
	cd->direction.r_size = (2 * cd->direction.max_lag + 1) * sizeof(int32_t);
	cd->direction.r = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, cd->direction.r_size);
	if (!cd->direction.r)
		goto err_free_all;

	/* Check for line array mode */
	cd->direction.line_array = line_array_mode_check(cd);

	/* Initialize direction to zero radians, set initial step sign to +1 */
	cd->direction.az = 0;
	cd->direction.step_sign = 1;
	return 0;

err_free_all:
	rfree(cd->direction.d);
	cd->direction.d = NULL;

err_free_iir:
	rfree(cd->direction.df2t_delay);
	cd->direction.df2t_delay = NULL;
	return -ENOMEM;
}

void tdfb_direction_free(struct tdfb_comp_data *cd)
{
	rfree(cd->direction.df2t_delay);
	rfree(cd->direction.d);
	rfree(cd->direction.r);
}

/* Measure level of one channel */
static void level_update(struct tdfb_comp_data *cd, int frames, int ch_count, int channel)
{
	int64_t tmp = 0;
	int64_t ambient;
	int64_t level;
	int16_t s;
	int16_t *p = cd->direction.rp + channel;
	int shift;
	int thr;
	int n;

	/* Calculate mean square level */
	for (n = 0; n < frames; n++) {
		s = *p;
		p += ch_count;
		tdfb_cinc_s16(&p, cd->direction.d_end, cd->direction.d_size);
		tmp += ((int64_t)s * s);
	}

	/* Calculate mean square power */
	cd->direction.level = sat_int32(tmp / frames);

	/* Slow track minimum level and use it as ambient noise level estimate */
	level = ((int64_t)cd->direction.level) << 32;
	ambient = cd->direction.level_ambient;
	shift = (level < ambient) ? FAST_LEVEL_SHIFT : SLOW_LEVEL_SHIFT;
	ambient = (level >> shift) - (ambient >> shift) + ambient;
	thr = sat_int32(Q_MULTS_32X32(ambient >> 32, POWER_THRESHOLD, 31, 10, 31)); /* int64_t */
	cd->direction.level_ambient = ambient;
	cd->direction.trigger <<= 1;
	if (cd->direction.level > thr)
		cd->direction.trigger |= 1;

	/* Levels update runs always when tracking, increment frame counter until wrap */
	cd->direction.frame_count_since_control += frames;
	if (cd->direction.frame_count_since_control < 0)
		cd->direction.frame_count_since_control = INT32_MAX;
}

static int find_max_value_index(int32_t *v, int length)
{
	int idx = 0;
	int i;

	for (i = 1; i < length; i++) {
		if (v[i] > v[idx])
			idx = i;
	}

	return idx;
}

static void time_differences(struct tdfb_comp_data *cd, int frames, int ch_count)
{
	int64_t r;
	int16_t *x;
	int16_t *y;
	int r_max_idx;
	int max_lag = cd->direction.max_lag;
	int c;
	int k;
	int i;
	int32_t maxval = 0;

	/* Calculate xcorr for channel 0 vs. 1 .. (ch_count-1). Scan -maxlag .. +maxlag*/
	for (c = 1; c < ch_count; c++) {
		for (k = -max_lag; k <= max_lag; k++) {
			y = cd->direction.rp; /* First channel */
			x = y + k * ch_count + c; /* other channel C */
			tdfb_cinc_s16(&x, cd->direction.d_end, cd->direction.d_size);
			tdfb_cdec_s16(&x, cd->direction.d, cd->direction.d_size);
			r = 0;
			for (i = 0; i < frames; i++) {
				if (maxval < *y)
					maxval = *y;

				r += (int32_t)*x * *y;
				y += ch_count;
				tdfb_cinc_s16(&y, cd->direction.d_end, cd->direction.d_size);
				x += ch_count;
				tdfb_cinc_s16(&x, cd->direction.d_end, cd->direction.d_size);
			}
			/* Scale for max. 20 ms 48 kHz frame */
			cd->direction.r[k + max_lag] = sat_int32(((r >> 8) + 1) >> 1);
		}
		r_max_idx = find_max_value_index(&cd->direction.r[0], 2 * max_lag + 1);
		cd->direction.timediff[c - 1] = (int32_t)(r_max_idx - max_lag) *
			cd->direction.unit_delay;
	}
	cd->direction.rp += frames * ch_count;
	tdfb_cinc_s16(&cd->direction.rp, cd->direction.d_end, cd->direction.d_size);
}

static int16_t distance_from_source(struct tdfb_comp_data *cd, int mic_n,
				    int16_t x, int16_t y, int16_t z)
{
	int32_t d2;
	int16_t dx;
	int16_t dy;
	int16_t dz;
	int16_t d;

	dx = x - cd->mic_locations[mic_n].x;
	dy = y - cd->mic_locations[mic_n].y;
	dz = z - cd->mic_locations[mic_n].z;

	/* d2 is Q8.24 meters */
	d2 = dx * dx + dy * dy + dz * dz;

	/* Squared distance is Q8.24, return Q4.12 meters */
	d = tdfb_mic_distance_sqrt(d2);
	return d;
}

static void theoretical_time_differences(struct tdfb_comp_data *cd, int16_t az)
{
	int16_t d[PLATFORM_MAX_CHANNELS];
	int16_t src_x;
	int16_t src_y;
	int16_t sin_az;
	int16_t cos_az;
	int32_t delta_d;
	int n_mic = cd->config->num_mic_locations;
	int i;

	sin_az = sin_fixed_16b(Q_SHIFT_LEFT((int32_t)az, 12, 28)); /* Q1.15 */
	cos_az = cos_fixed_16b(Q_SHIFT_LEFT((int32_t)az, 12, 28)); /* Q1.15 */
	src_x = Q_MULTSR_16X16((int32_t)cos_az, SOURCE_DISTANCE, 15, 12, 12);
	src_y = Q_MULTSR_16X16((int32_t)sin_az, SOURCE_DISTANCE, 15, 12, 12);

	for (i = 0; i < n_mic; i++)
		d[i] = distance_from_source(cd, i, src_x, src_y, 0);

	for (i = 0; i < n_mic - 1; i++) {
		delta_d = d[i + 1] - d[0]; /* Meters Q4.12 */
		cd->direction.timediff_iter[i] =
			(int32_t)((((int64_t)delta_d) << 19) / SPEED_OF_SOUND);
	}
}

static int64_t mean_square_time_difference_err(struct tdfb_comp_data *cd)
{
	int64_t err = 0;
	int32_t delta;
	int i;

	for (i = 0; i < cd->config->num_mic_locations - 1; i++) {
		delta = cd->direction.timediff[i] - cd->direction.timediff_iter[i];
		err +=  (int64_t)delta * delta;
	}

	return err;
}

static int unwrap_radians(int radians)
{
	int a = radians;

	if (a > PI_Q12)
		a -= PIMUL2_Q12;

	if (a < -PI_Q12)
		a += PIMUL2_Q12;

	return a;
}

static void iterate_source_angle(struct tdfb_comp_data *cd)
{
	int64_t err_prev;
	int64_t err;
	int32_t ds1;
	int32_t ds2;
	int az_slow;
	int i;
	int az_step = AZ_STEP * cd->direction.step_sign;
	int az = cd->direction.az_slow;

	/* Start next iteration opposite direction */
	cd->direction.step_sign *= -1;

	/* Get theoretical time differences for previous angle */
	theoretical_time_differences(cd, az);
	err_prev = mean_square_time_difference_err(cd);

	for (i = 0; i < AZ_ITERATIONS; i++) {
		az += az_step;
		theoretical_time_differences(cd, az);
		err = mean_square_time_difference_err(cd);
		if (err > err_prev) {
			az_step = -(az_step >> 1);
			if (az_step == 0)
				break;
		}

		err_prev = err;
	}

	az = unwrap_radians(az);
	if (cd->direction.line_array) {
		/* Line array azimuth angle is -90 .. +90 */
		if (az > PIDIV2_Q12)
			az = PI_Q12 - az;

		if (az < -PIDIV2_Q12)
			az = -PI_Q12  - az;
	}

	cd->direction.az = az;

	/* Avoid low-pass filtering to zero the angle in 360 degree arrays
	 * due to discontinuity at -180 or +180 degree point and estimation
	 * noise. Try to camp on either side of the circle.
	 */
	ds1 = cd->direction.az_slow - az;
	ds1 = ds1 * ds1;
	ds2 = cd->direction.az_slow - (az - 360);
	ds2 = ds2 * ds2;
	if (ds2 < ds1) {
		az -= 360;
	} else {
		ds2 = cd->direction.az_slow - (az + 360);
		ds2 = ds2 * ds2;
		if (ds2 < ds1)
			az += 360;
	}

	az_slow = Q_MULTSR_16X16((int32_t)az, SLOW_AZ_C1, 12, 15, 12) +
		Q_MULTSR_16X16((int32_t)cd->direction.az_slow, SLOW_AZ_C2, 12, 15, 12);
	cd->direction.az_slow = unwrap_radians(az_slow);
}

static void updates_when_no_trigger(struct tdfb_comp_data *cd, int frames, int ch_count)
{
	cd->direction.rp += frames * ch_count;
	tdfb_cinc_s16(&cd->direction.rp, cd->direction.d_end, cd->direction.d_size);
}

static int convert_angle_to_enum(struct tdfb_comp_data *cd)
{
	int16_t deg;
	int new_az_value;

	/* Update azimuth enum with radians to degrees to enum step conversion. First
	 * convert radians to deg, subtract angle offset, and make angles positive.
	 */
	deg = Q_MULTS_16X16((int32_t)cd->direction.az_slow, COEF_RAD_TO_DEG, 12, 9, 0) -
		cd->config->angle_enum_offs;

	/* Divide and round to enum angle scale, remove duplicate 0 and 360 degree angle
	 * representation to have single zero degree enum.
	 */
	new_az_value = ((2 * deg / cd->config->angle_enum_mult) + 1) >> 1;
	if (new_az_value * cd->config->angle_enum_mult == 360)
		new_az_value -= 360 / cd->config->angle_enum_mult;

	return new_az_value;
}

/* Placeholder function for sound direction estimate */
void tdfb_direction_estimate(struct tdfb_comp_data *cd, int frames, int ch_count)
{
	int32_t time_since;
	int new_az_value;

	if (!cd->direction_updates)
		return;

	/* Update levels, skip rest of estimation if level does not exceed well ambient */
	level_update(cd, frames, ch_count, 0);
	if (!(cd->direction.trigger & 1)) {
		updates_when_no_trigger(cd, frames, ch_count);
		return;
	}

	/* Compute time differences of ch_count vs. reference channel 1 */
	time_differences(cd, frames, ch_count);

	/* Determine direction angle */
	iterate_source_angle(cd);

	/* Convert radians to enum*/
	new_az_value = convert_angle_to_enum(cd);

	/* Time after last control as Q16.16, unit delay is Q1.131, count is Q0. */
	time_since = Q_MULTS_32X32((int64_t)cd->direction.unit_delay,
				   cd->direction.frame_count_since_control, 31, 0, 16);

	/* Need a new enum value, sufficient time since last update and four measurement frames
	 * to update user space.
	 */
	if (new_az_value != cd->az_value_estimate && time_since > CONTROL_UPDATE_MIN_TIME &&
	    (cd->direction.trigger & 0x15) == 0x15) {
		cd->az_value_estimate = new_az_value;
		cd->direction.frame_count_since_control = 0;
		cd->direction_change = true;
	}
}
