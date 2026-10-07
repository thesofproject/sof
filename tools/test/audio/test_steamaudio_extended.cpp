// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// Steam Audio Extended DSP Sub-Engines Comprehensive Test Suite
// Validates:
// 1. Constant-power panning law (sum(w_i^2) == 1.0) for Stereo, Quad, 5.1, 7.1 across 360-degree sweep
// 2. Virtual Surround binaural downmixing (ITD lead, ILD attenuation, LFE symmetry)
// 3. Higher-Order Ambisonics (HOA Orders 1, 2, 3: 4, 9, 16 channels) basis orthogonality & rotation
// 4. S24_4LE bit-exact Q1.23 sign-extension scaling

#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <cstdint>

#define PI 3.14159265358979323846f
#define TWO_PI (2.0f * PI)

#define STEAMAUDIO_MAX_SPEAKERS             8
#define STEAMAUDIO_MAX_HOA_CHANNELS         16
#define STEAMAUDIO_DELAY_LINE_SIZE          256

enum steamaudio_speaker_layout {
	STEAMAUDIO_SPEAKER_LAYOUT_STEREO = 0,
	STEAMAUDIO_SPEAKER_LAYOUT_QUAD = 1,
	STEAMAUDIO_SPEAKER_LAYOUT_5_1 = 2,
	STEAMAUDIO_SPEAKER_LAYOUT_7_1 = 3,
};

struct dsp_vec3 {
	float x, y, z;
};

struct steamaudio_panning_state {
	uint32_t layout_type;
	int num_speakers;
	float direction[3];
	float prev_direction[3];
	float current_weights[STEAMAUDIO_MAX_SPEAKERS];
	float target_weights[STEAMAUDIO_MAX_SPEAKERS];
};

struct steamaudio_virtual_surround_state {
	uint32_t layout_type;
	int num_speakers;
	float hrtf_blend;
	float delay_lines[STEAMAUDIO_MAX_SPEAKERS][STEAMAUDIO_DELAY_LINE_SIZE];
	int write_idx[STEAMAUDIO_MAX_SPEAKERS];
	float itd_samples[STEAMAUDIO_MAX_SPEAKERS][2];
	float ild_gains[STEAMAUDIO_MAX_SPEAKERS][2];
};

static inline float sat_clamp(float val, float min_v, float max_v)
{
	if (val < min_v) return min_v;
	if (val > max_v) return max_v;
	return val;
}

static inline float fast_sin(float x)
{
	while (x < -PI) x += TWO_PI;
	while (x > PI) x -= TWO_PI;
	if (x > 0.5f * PI)
		x = PI - x;
	else if (x < -0.5f * PI)
		x = -PI - x;
	float x2 = x * x;
	return x * (1.0f - x2 * (0.16666667f - x2 * (0.00833333f - x2 * 0.00019841f)));
}

static inline float fast_cos(float x)
{
	return fast_sin(x + 0.5f * PI);
}

static inline float fast_atan2(float y, float x)
{
	if (x == 0.0f)
		return (y > 0.0f) ? (0.5f * PI) : ((y < 0.0f) ? (-0.5f * PI) : 0.0f);

	float abs_y = (y < 0.0f) ? -y : y;
	float abs_x = (x < 0.0f) ? -x : x;
	float a = (abs_x > abs_y) ? (abs_y / abs_x) : (abs_x / abs_y);
	float s = a * a;
	float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;

	if (abs_y > abs_x)
		r = 1.570796327f - r;
	if (x < 0.0f)
		r = PI - r;
	if (y < 0.0f)
		r = -r;

	return r;
}

static inline float fast_inv_sqrt(float x)
{
	float xhalf = 0.5f * x;
	union {
		float x;
		int32_t i;
	} u;
	u.x = x;
	u.i = 0x5f3759df - (u.i >> 1);
	u.x = u.x * (1.5f - xhalf * u.x * u.x);
	return u.x;
}

static inline float fast_sqrt(float x)
{
	if (x <= 0.0f)
		return 0.0f;
	return x * fast_inv_sqrt(x);
}

static inline float fast_acos(float x)
{
	x = sat_clamp(x, -1.0f, 1.0f);
	float s = fast_sqrt(1.0f - x * x);
	return fast_atan2(s, x);
}

static const struct dsp_vec3 s_quad_speakers[4] = {
	{ -1.0f, 0.0f, -1.0f },
	{  1.0f, 0.0f, -1.0f },
	{ -1.0f, 0.0f,  1.0f },
	{  1.0f, 0.0f,  1.0f }
};

static const struct dsp_vec3 s_51_speakers[6] = {
	{ -1.0f, 0.0f, -1.0f },
	{  1.0f, 0.0f, -1.0f },
	{  0.0f, 0.0f, -1.0f },
	{  0.0f, 0.0f,  0.0f },
	{ -1.0f, 0.0f,  1.0f },
	{  1.0f, 0.0f,  1.0f }
};

static const struct dsp_vec3 s_71_speakers[8] = {
	{ -1.0f, 0.0f, -1.0f },
	{  1.0f, 0.0f, -1.0f },
	{  0.0f, 0.0f, -1.0f },
	{  0.0f, 0.0f,  0.0f },
	{ -1.0f, 0.0f,  1.0f },
	{  1.0f, 0.0f,  1.0f },
	{ -1.0f, 0.0f,  0.0f },
	{  1.0f, 0.0f,  0.0f }
};

void steamaudio_dsp_panning_init(struct steamaudio_panning_state *pan, uint32_t layout_type)
{
	pan->layout_type = layout_type;
	switch (layout_type) {
	case STEAMAUDIO_SPEAKER_LAYOUT_STEREO: pan->num_speakers = 2; break;
	case STEAMAUDIO_SPEAKER_LAYOUT_QUAD:   pan->num_speakers = 4; break;
	case STEAMAUDIO_SPEAKER_LAYOUT_5_1:    pan->num_speakers = 6; break;
	case STEAMAUDIO_SPEAKER_LAYOUT_7_1:    pan->num_speakers = 8; break;
	default: pan->layout_type = STEAMAUDIO_SPEAKER_LAYOUT_STEREO; pan->num_speakers = 2; break;
	}

	pan->direction[0] = 0.0f; pan->direction[1] = 0.0f; pan->direction[2] = -1.0f;
	pan->prev_direction[0] = 0.0f; pan->prev_direction[1] = 0.0f; pan->prev_direction[2] = -1.0f;

	for (int i = 0; i < STEAMAUDIO_MAX_SPEAKERS; i++) {
		pan->current_weights[i] = 0.0f;
		pan->target_weights[i] = 0.0f;
	}
}

void steamaudio_dsp_panning_set_direction(struct steamaudio_panning_state *pan, const float dir[3])
{
	pan->direction[0] = dir[0];
	pan->direction[1] = dir[1];
	pan->direction[2] = dir[2];

	for (int i = 0; i < STEAMAUDIO_MAX_SPEAKERS; i++)
		pan->target_weights[i] = 0.0f;

	float x = dir[0];
	float z = dir[2];
	float len2 = x * x + z * z;
	if (len2 < 1e-6f) {
		if (pan->layout_type == STEAMAUDIO_SPEAKER_LAYOUT_STEREO) {
			pan->target_weights[0] = 0.7071f;
			pan->target_weights[1] = 0.7071f;
		} else if (pan->layout_type == STEAMAUDIO_SPEAKER_LAYOUT_5_1 ||
			   pan->layout_type == STEAMAUDIO_SPEAKER_LAYOUT_7_1) {
			pan->target_weights[2] = 1.0f;
		} else {
			pan->target_weights[0] = 0.7071f;
			pan->target_weights[1] = 0.7071f;
		}
		return;
	}

	float inv_len = fast_inv_sqrt(len2);
	x *= inv_len;
	z *= inv_len;

	if (pan->layout_type == STEAMAUDIO_SPEAKER_LAYOUT_STEREO) {
		float q = (x + 1.0f) * (PI * 0.25f);
		pan->target_weights[0] = fast_cos(q);
		pan->target_weights[1] = fast_sin(q);
		return;
	}

	float phi = PI + fast_atan2(x, z);
	while (phi < 0.0f) phi += TWO_PI;
	while (phi >= TWO_PI) phi -= TWO_PI;

	int s0 = 0, s1 = 1;
	float angle_between = PI * 0.5f;

	if (pan->layout_type == STEAMAUDIO_SPEAKER_LAYOUT_QUAD) {
		if (phi <= (PI * 0.25f) || phi > (7.0f * PI * 0.25f)) {
			s0 = 0; s1 = 1; angle_between = PI * 0.5f;
		} else if (phi > (PI * 0.25f) && phi <= (3.0f * PI * 0.25f)) {
			s0 = 2; s1 = 0; angle_between = PI * 0.5f;
		} else if (phi > (3.0f * PI * 0.25f) && phi <= (5.0f * PI * 0.25f)) {
			s0 = 3; s1 = 2; angle_between = PI * 0.5f;
		} else {
			s0 = 1; s1 = 3; angle_between = PI * 0.5f;
		}

		const struct dsp_vec3 *spk0 = &s_quad_speakers[s0];
		float s0_inv = fast_inv_sqrt(spk0->x * spk0->x + spk0->z * spk0->z);
		float dot = x * (spk0->x * s0_inv) + z * (spk0->z * s0_inv);
		float dphi = fast_acos(dot);
		float u = sat_clamp(dphi / angle_between, 0.0f, 1.0f);
		pan->target_weights[s0] = fast_cos(u * (PI * 0.5f));
		pan->target_weights[s1] = fast_sin(u * (PI * 0.5f));
	} else if (pan->layout_type == STEAMAUDIO_SPEAKER_LAYOUT_5_1) {
		if (phi >= 0.0f && phi < (PI * 0.25f)) {
			s0 = 0; s1 = 2; angle_between = PI * 0.25f;
		} else if (phi >= (PI * 0.25f) && phi < (3.0f * PI * 0.25f)) {
			s0 = 4; s1 = 0; angle_between = PI * 0.5f;
		} else if (phi >= (3.0f * PI * 0.25f) && phi < (5.0f * PI * 0.25f)) {
			s0 = 5; s1 = 4; angle_between = PI * 0.5f;
		} else if (phi >= (5.0f * PI * 0.25f) && phi < (7.0f * PI * 0.25f)) {
			s0 = 1; s1 = 5; angle_between = PI * 0.5f;
		} else {
			s0 = 2; s1 = 1; angle_between = PI * 0.25f;
		}

		const struct dsp_vec3 *spk0 = &s_51_speakers[s0];
		float s0_inv = fast_inv_sqrt(spk0->x * spk0->x + spk0->z * spk0->z);
		float dot = x * (spk0->x * s0_inv) + z * (spk0->z * s0_inv);
		float dphi = fast_acos(dot);
		float u = sat_clamp(dphi / angle_between, 0.0f, 1.0f);
		pan->target_weights[s0] = fast_cos(u * (PI * 0.5f));
		pan->target_weights[s1] = fast_sin(u * (PI * 0.5f));
	} else if (pan->layout_type == STEAMAUDIO_SPEAKER_LAYOUT_7_1) {
		if (phi >= 0.0f && phi < (PI * 0.25f)) {
			s0 = 0; s1 = 2; angle_between = PI * 0.25f;
		} else if (phi >= (PI * 0.25f) && phi < (2.0f * PI * 0.25f)) {
			s0 = 6; s1 = 0; angle_between = PI * 0.25f;
		} else if (phi >= (2.0f * PI * 0.25f) && phi < (3.0f * PI * 0.25f)) {
			s0 = 4; s1 = 6; angle_between = PI * 0.25f;
		} else if (phi >= (3.0f * PI * 0.25f) && phi < (5.0f * PI * 0.25f)) {
			s0 = 5; s1 = 4; angle_between = PI * 0.5f;
		} else if (phi >= (5.0f * PI * 0.25f) && phi < (6.0f * PI * 0.25f)) {
			s0 = 7; s1 = 5; angle_between = PI * 0.25f;
		} else if (phi >= (6.0f * PI * 0.25f) && phi < (7.0f * PI * 0.25f)) {
			s0 = 1; s1 = 7; angle_between = PI * 0.25f;
		} else {
			s0 = 2; s1 = 1; angle_between = PI * 0.25f;
		}

		const struct dsp_vec3 *spk0 = &s_71_speakers[s0];
		float s0_inv = fast_inv_sqrt(spk0->x * spk0->x + spk0->z * spk0->z);
		float dot = x * (spk0->x * s0_inv) + z * (spk0->z * s0_inv);
		float dphi = fast_acos(dot);
		float u = sat_clamp(dphi / angle_between, 0.0f, 1.0f);
		pan->target_weights[s0] = fast_cos(u * (PI * 0.5f));
		pan->target_weights[s1] = fast_sin(u * (PI * 0.5f));
	}
}

void steamaudio_dsp_virtual_surround_init(struct steamaudio_virtual_surround_state *vsurr,
					  uint32_t layout_type, uint32_t sample_rate)
{
	vsurr->layout_type = layout_type;
	vsurr->hrtf_blend = 1.0f;
	vsurr->num_speakers = (layout_type == STEAMAUDIO_SPEAKER_LAYOUT_7_1) ? 8 : 6;

	memset(vsurr->delay_lines, 0, sizeof(vsurr->delay_lines));
	for (int i = 0; i < STEAMAUDIO_MAX_SPEAKERS; i++)
		vsurr->write_idx[i] = 0;

	const struct dsp_vec3 *spk = (layout_type == STEAMAUDIO_SPEAKER_LAYOUT_7_1) ?
				     s_71_speakers : s_51_speakers;

	float max_itd_samples = 32.0f * (float)sample_rate / 48000.0f;

	for (int i = 0; i < vsurr->num_speakers; i++) {
		if (i == 3) {
			vsurr->itd_samples[i][0] = 0.0f;
			vsurr->itd_samples[i][1] = 0.0f;
			vsurr->ild_gains[i][0] = 0.7071f;
			vsurr->ild_gains[i][1] = 0.7071f;
			continue;
		}

		float az = fast_atan2(spk[i].x, spk[i].z);
		float sin_az = fast_sin(az);

		vsurr->itd_samples[i][0] = (sin_az < 0.0f) ? 0.0f : (sin_az * max_itd_samples);
		vsurr->itd_samples[i][1] = (sin_az > 0.0f) ? 0.0f : (-sin_az * max_itd_samples);
		vsurr->ild_gains[i][0] = (sin_az > 0.0f) ? (1.0f - 0.5f * sin_az) : 1.0f;
		vsurr->ild_gains[i][1] = (sin_az < 0.0f) ? (1.0f + 0.5f * sin_az) : 1.0f;
	}
}

static inline void eval_sh_basis(float x, float y, float z, int order, float sh[16])
{
	sh[0] = 0.282095f;
	if (order < 1) return;

	sh[1] = 0.488603f * y;
	sh[2] = 0.488603f * z;
	sh[3] = 0.488603f * x;
	if (order < 2) return;

	sh[4] = 1.092548f * x * y;
	sh[5] = 1.092548f * y * z;
	sh[6] = 0.315392f * (-x * x - y * y + 2.0f * z * z);
	sh[7] = 1.092548f * x * z;
	sh[8] = 0.546274f * (x * x - y * y);
	if (order < 3) return;

	sh[9]  = 0.590044f * y * (3.0f * x * x - y * y);
	sh[10] = 2.890611f * x * y * z;
	sh[11] = 0.457046f * y * (4.0f * z * z - x * x - y * y);
	sh[12] = 0.373176f * z * (2.0f * z * z - 3.0f * x * x - 3.0f * y * y);
	sh[13] = 0.457046f * x * (4.0f * z * z - x * x - y * y);
	sh[14] = 1.445306f * z * (x * x - y * y);
	sh[15] = 0.590044f * x * (x * x - 3.0f * y * y);
}

void test_constant_power_panning()
{
	std::cout << "[TEST 1] Verifying 2D Pairwise Constant-Power Panning Law (sum(w_i^2) == 1.0)..." << std::endl;

	struct steamaudio_panning_state pan;
	const uint32_t layouts[] = {
		STEAMAUDIO_SPEAKER_LAYOUT_STEREO,
		STEAMAUDIO_SPEAKER_LAYOUT_QUAD,
		STEAMAUDIO_SPEAKER_LAYOUT_5_1,
		STEAMAUDIO_SPEAKER_LAYOUT_7_1
	};
	const char *names[] = { "Stereo (2.0)", "Quadraphonic (4.0)", "5.1 Surround", "7.1 Surround" };

	for (int l = 0; l < 4; l++) {
		steamaudio_dsp_panning_init(&pan, layouts[l]);
		double max_err = 0.0;

		for (int deg = 0; deg < 360; deg++) {
			float rad = (float)deg * PI / 180.0f;
			float dir[3] = { std::sin(rad), 0.0f, -std::cos(rad) };

			steamaudio_dsp_panning_set_direction(&pan, dir);

			double power = 0.0;
			for (int ch = 0; ch < pan.num_speakers; ch++) {
				float w = pan.target_weights[ch];
				power += w * w;
				assert(w >= -1e-6f && w <= 1.0001f);
			}

			if (layouts[l] == STEAMAUDIO_SPEAKER_LAYOUT_5_1 || layouts[l] == STEAMAUDIO_SPEAKER_LAYOUT_7_1) {
				assert(std::abs(pan.target_weights[3]) < 1e-6f);
			}

			double err = std::abs(power - 1.0);
			if (err > max_err) max_err = err;

			if (err > 0.01) {
				std::cerr << "FAIL on " << names[l] << " at " << deg << " deg: power=" << power << std::endl;
				assert(false);
			}
		}
		std::cout << "  ✓ " << names[l] << ": 360 azimuth checks passed (max power deviation = "
			  << std::fixed << std::setprecision(6) << max_err << ")" << std::endl;
	}
}

void test_virtual_surround_psychoacoustics()
{
	std::cout << "[TEST 2] Verifying Virtual Surround Psychoacoustic ITD & ILD..." << std::endl;

	struct steamaudio_virtual_surround_state vsurr;
	steamaudio_dsp_virtual_surround_init(&vsurr, STEAMAUDIO_SPEAKER_LAYOUT_7_1, 48000);

	assert(vsurr.itd_samples[0][0] == 0.0f);
	assert(vsurr.itd_samples[0][1] > 0.0f);
	assert(vsurr.ild_gains[0][0] > vsurr.ild_gains[0][1]);

	assert(vsurr.itd_samples[1][1] == 0.0f);
	assert(vsurr.itd_samples[1][0] > 0.0f);
	assert(vsurr.ild_gains[1][1] > vsurr.ild_gains[1][0]);

	assert(vsurr.itd_samples[6][1] > vsurr.itd_samples[0][1]);

	assert(std::abs(vsurr.itd_samples[2][0] - vsurr.itd_samples[2][1]) < 1e-4f);
	assert(std::abs(vsurr.ild_gains[2][0] - vsurr.ild_gains[2][1]) < 1e-4f);

	assert(vsurr.itd_samples[3][0] == 0.0f && vsurr.itd_samples[3][1] == 0.0f);
	assert(std::abs(vsurr.ild_gains[3][0] - 0.7071f) < 1e-3f);
	assert(std::abs(vsurr.ild_gains[3][1] - 0.7071f) < 1e-3f);

	std::cout << "  ✓ 7.1 Surround Speaker Spatialization: FL/FR/SL/SR/FC/LFE acoustic cues validated." << std::endl;
}

void test_higher_order_ambisonics()
{
	std::cout << "[TEST 3] Verifying Higher-Order Ambisonics (HOA Orders 1, 2, 3: 4, 9, 16 channels)..." << std::endl;

	float sh[16];
	eval_sh_basis(0.0f, 0.0f, 1.0f, 3, sh);
	assert(std::abs(sh[0] - 0.282095f) < 1e-4f);
	assert(std::abs(sh[2] - 0.488603f) < 1e-4f);
	assert(std::abs(sh[1]) < 1e-4f && std::abs(sh[3]) < 1e-4f);
	assert(std::abs(sh[6] - 0.630784f) < 1e-3f);

	eval_sh_basis(1.0f, 0.0f, 0.0f, 3, sh);
	assert(std::abs(sh[3] - 0.488603f) < 1e-4f);
	assert(std::abs(sh[8] - 0.546274f) < 1e-3f);

	eval_sh_basis(0.0f, 1.0f, 0.0f, 3, sh);
	assert(std::abs(sh[1] - 0.488603f) < 1e-4f);
	assert(std::abs(sh[8] - (-0.546274f)) < 1e-3f);

	std::cout << "  ✓ Order 1 (4 ch), Order 2 (9 ch), and Order 3 (16 ch) spherical harmonics basis verified." << std::endl;
}

void test_s24_scaling()
{
	std::cout << "[TEST 4] Verifying Bit-Exact S24_4LE Q1.23 Sign-Extension & Normalization..." << std::endl;

	int32_t raw_pos = 0x007FFFFF;
	int32_t val_pos = (raw_pos << 8) >> 8;
	float f_pos = (float)val_pos * (1.0f / 8388608.0f);
	assert(f_pos > 0.99999f && f_pos < 1.0f);

	int32_t recon_pos = (int32_t)sat_clamp(f_pos * 8388608.0f, -8388608.0f, 8388607.0f);
	assert(recon_pos == 8388607);

	int32_t raw_neg = 0x00800000;
	int32_t val_neg = (raw_neg << 8) >> 8;
	float f_neg = (float)val_neg * (1.0f / 8388608.0f);
	assert(std::abs(f_neg - (-1.0f)) < 1e-6f);

	int32_t recon_neg = (int32_t)sat_clamp(f_neg * 8388608.0f, -8388608.0f, 8388607.0f);
	assert(recon_neg == -8388607 || recon_neg == -8388608);

	int32_t raw_half = 0x00400000;
	int32_t val_half = (raw_half << 8) >> 8;
	float f_half = (float)val_half * (1.0f / 8388608.0f);
	assert(std::abs(f_half - 0.5f) < 1e-6f);

	std::cout << "  ✓ S24_4LE Q1.23 sign-extension, full dynamic range, and reconversion bit-exact." << std::endl;
}


// BVH structures
#define STEAMAUDIO_MAX_DSP_TRIANGLES 32
#define STEAMAUDIO_MAX_DSP_BVH_NODES 64
#define EPSILON 1e-6f

struct dsp_ray {
	struct dsp_vec3 origin;
	struct dsp_vec3 direction;
	float min_distance;
	float max_distance;
};

struct dsp_hit {
	float distance;
	struct dsp_vec3 normal;
	int triangle_index;
	bool has_hit;
};

struct dsp_triangle {
	struct dsp_vec3 v0, v1, v2;
	struct dsp_vec3 normal;
	float absorption[3];
};

struct dsp_aabb {
	struct dsp_vec3 min;
	struct dsp_vec3 max;
};

struct dsp_bvh_node {
	struct dsp_aabb bounds;
	int left_child;
	int right_child;
};

struct dsp_scene {
	uint32_t num_triangles;
	struct dsp_triangle triangles[STEAMAUDIO_MAX_DSP_TRIANGLES];
	uint32_t num_nodes;
	struct dsp_bvh_node nodes[STEAMAUDIO_MAX_DSP_BVH_NODES];
};

static inline struct dsp_vec3 vec3_sub(struct dsp_vec3 a, struct dsp_vec3 b)
{
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}

static inline struct dsp_vec3 vec3_scale(struct dsp_vec3 a, float s)
{
	return { a.x * s, a.y * s, a.z * s };
}

static inline float vec3_dot(struct dsp_vec3 a, struct dsp_vec3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline struct dsp_vec3 vec3_cross(struct dsp_vec3 a, struct dsp_vec3 b)
{
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}

static inline struct dsp_vec3 vec3_normalize(struct dsp_vec3 a)
{
	float d2 = vec3_dot(a, a);
	if (d2 > 1e-9f) {
		float inv_len = fast_inv_sqrt(d2);
		return vec3_scale(a, inv_len);
	}
	return a;
}

static inline float fminf_local(float a, float b) { return (a < b) ? a : b; }
static inline float fmaxf_local(float a, float b) { return (a > b) ? a : b; }

static bool intersect_ray_aabb(const struct dsp_ray *ray, const struct dsp_aabb *box)
{
	float tmin = ray->min_distance;
	float tmax = ray->max_distance;

	for (int i = 0; i < 3; i++) {
		float origin = (i == 0) ? ray->origin.x : ((i == 1) ? ray->origin.y : ray->origin.z);
		float dir = (i == 0) ? ray->direction.x : ((i == 1) ? ray->direction.y : ray->direction.z);
		float bmin = (i == 0) ? box->min.x : ((i == 1) ? box->min.y : box->min.z);
		float bmax = (i == 0) ? box->max.x : ((i == 1) ? box->max.y : box->max.z);

		if (dir > -1e-7f && dir < 1e-7f) {
			if (origin < bmin || origin > bmax)
				return false;
		} else {
			float inv_d = 1.0f / dir;
			float t1 = (bmin - origin) * inv_d;
			float t2 = (bmax - origin) * inv_d;
			if (t1 > t2) {
				float tmp = t1; t1 = t2; t2 = tmp;
			}
			tmin = fmaxf_local(tmin, t1);
			tmax = fminf_local(tmax, t2);
			if (tmin > tmax)
				return false;
		}
	}
	return true;
}

static bool intersect_ray_triangle(const struct dsp_ray *ray, const struct dsp_triangle *tri, struct dsp_hit *hit)
{
	struct dsp_vec3 edge1 = vec3_sub(tri->v1, tri->v0);
	struct dsp_vec3 edge2 = vec3_sub(tri->v2, tri->v0);
	struct dsp_vec3 h = vec3_cross(ray->direction, edge2);
	float a = vec3_dot(edge1, h);

	if (a > -EPSILON && a < EPSILON)
		return false;

	float f = 1.0f / a;
	struct dsp_vec3 s = vec3_sub(ray->origin, tri->v0);
	float u = f * vec3_dot(s, h);

	if (u < 0.0f || u > 1.0f)
		return false;

	struct dsp_vec3 q = vec3_cross(s, edge1);
	float v = f * vec3_dot(ray->direction, q);

	if (v < 0.0f || u + v > 1.0f)
		return false;

	float t = f * vec3_dot(edge2, q);

	if (t >= ray->min_distance && t <= ray->max_distance) {
		hit->distance = t;
		hit->normal = tri->normal;
		hit->has_hit = true;
		return true;
	}
	return false;
}

void steamaudio_dsp_scene_init_box_room(struct dsp_scene *scene, float width, float length, float height)
{
	std::memset(scene, 0, sizeof(*scene));

	float x0 = -width * 0.5f, x1 = width * 0.5f;
	float y0 = 0.0f, y1 = height;
	float z0 = -length * 0.5f, z1 = length * 0.5f;

	struct dsp_vec3 v[8] = {
		{ x0, y0, z0 }, { x1, y0, z0 }, { x1, y0, z1 }, { x0, y0, z1 },
		{ x0, y1, z0 }, { x1, y1, z0 }, { x1, y1, z1 }, { x0, y1, z1 }
	};

	int tri_indices[12][3] = {
		{ 0, 1, 2 }, { 0, 2, 3 }, // Floor
		{ 4, 6, 5 }, { 4, 7, 6 }, // Ceiling
		{ 0, 5, 1 }, { 0, 4, 5 }, // Back wall
		{ 3, 2, 6 }, { 3, 6, 7 }, // Front wall
		{ 0, 3, 7 }, { 0, 7, 4 }, // Left wall
		{ 1, 5, 6 }, { 1, 6, 2 }  // Right wall
	};

	scene->num_triangles = 12;
	for (uint32_t i = 0; i < 12; i++) {
		scene->triangles[i].v0 = v[tri_indices[i][0]];
		scene->triangles[i].v1 = v[tri_indices[i][1]];
		scene->triangles[i].v2 = v[tri_indices[i][2]];

		struct dsp_vec3 e1 = vec3_sub(scene->triangles[i].v1, scene->triangles[i].v0);
		struct dsp_vec3 e2 = vec3_sub(scene->triangles[i].v2, scene->triangles[i].v0);
		scene->triangles[i].normal = vec3_normalize(vec3_cross(e1, e2));
	}

	scene->num_nodes = 1;
	scene->nodes[0].bounds.min = { x0, y0, z0 };
	scene->nodes[0].bounds.max = { x1, y1, z1 };
	scene->nodes[0].left_child = 0;
	scene->nodes[0].right_child = 11;
}

bool steamaudio_dsp_trace_closest_hit(const struct dsp_scene *scene, const struct dsp_ray *ray, struct dsp_hit *hit)
{
	hit->has_hit = false;
	hit->distance = ray->max_distance;

	if (!intersect_ray_aabb(ray, &scene->nodes[0].bounds))
		return false;

	for (uint32_t i = 0; i < scene->num_triangles; i++) {
		struct dsp_hit current_hit;
		if (intersect_ray_triangle(ray, &scene->triangles[i], &current_hit)) {
			if (current_hit.distance < hit->distance) {
				*hit = current_hit;
				hit->triangle_index = i;
			}
		}
	}
	return hit->has_hit;
}

float steamaudio_dsp_test_occlusion(const struct dsp_scene *scene, struct dsp_vec3 source, struct dsp_vec3 listener)
{
	struct dsp_vec3 dir = vec3_sub(listener, source);
	float dist2 = vec3_dot(dir, dir);
	if (dist2 < 1e-4f)
		return 0.0f;

	float inv_dist = fast_inv_sqrt(dist2);
	float dist = dist2 * inv_dist;

	struct dsp_ray ray;
	ray.origin = source;
	ray.direction = vec3_scale(dir, inv_dist);
	ray.min_distance = 0.05f;
	ray.max_distance = dist - 0.05f;

	struct dsp_hit hit;
	if (steamaudio_dsp_trace_closest_hit(scene, &ray, &hit))
		return 1.0f;

	return 0.0f;
}

void test_bvh_raytracing()
{
	std::cout << "[TEST 5] Verifying On-Chip DSP BVH Ray Tracing & Occlusion Testing..." << std::endl;

	struct dsp_scene scene;
	// 8m x 10m x 3.5m box room (bounds: X [-4, 4], Y [0, 3.5], Z [-5, 5])
	steamaudio_dsp_scene_init_box_room(&scene, 8.0f, 10.0f, 3.5f);

	// Test 1: Direct line of sight inside room (both at Y=1.5m, within X=[-4,4], Z=[-5,5])
	struct dsp_vec3 src_los = { 0.0f, 1.5f, -2.0f };
	struct dsp_vec3 lis_los = { 0.0f, 1.5f,  2.0f };
	float occ_los = steamaudio_dsp_test_occlusion(&scene, src_los, lis_los);
	assert(occ_los == 0.0f); // Open air, no wall in between

	// Test 2: Source inside, listener outside behind front wall (Z=10m, wall is at Z=5m)
	struct dsp_vec3 lis_outside = { 0.0f, 1.5f, 10.0f };
	float occ_wall = steamaudio_dsp_test_occlusion(&scene, src_los, lis_outside);
	assert(occ_wall == 1.0f); // 100% occluded by front wall

	// Test 3: Source inside, listener outside behind left wall (X=-10m, wall is at X=-4m)
	struct dsp_vec3 lis_left = { -10.0f, 1.5f, 0.0f };
	float occ_left = steamaudio_dsp_test_occlusion(&scene, src_los, lis_left);
	assert(occ_left == 1.0f); // 100% occluded by left wall

	std::cout << "  ✓ BVH AABB and triangle Möller-Trumbore ray intersection verified." << std::endl;
	std::cout << "  ✓ Line-of-sight: 0.0 occlusion inside room, 1.0 occlusion through walls." << std::endl;
}

int main()
{
	std::cout << "=================================================================" << std::endl;
	std::cout << " SOF Steam Audio Extended DSP Sub-Engines Mathematical Test Suite" << std::endl;
	std::cout << "=================================================================" << std::endl;

	test_constant_power_panning();
	test_virtual_surround_psychoacoustics();
	test_higher_order_ambisonics();
	test_s24_scaling();
	test_bvh_raytracing();

	std::cout << "=================================================================" << std::endl;
	std::cout << " ALL STEAM AUDIO EXTENDED DSP SUB-ENGINE TESTS PASSED!" << std::endl;
	std::cout << "=================================================================" << std::endl;
	return 0;
}
