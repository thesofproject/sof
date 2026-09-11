// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Steam Audio Lightweight DSP BVH Ray Tracer

#include "steamaudio.h"
#include <rtos/string.h>

#define EPSILON 1e-6f

static inline struct dsp_vec3 vec3_sub(struct dsp_vec3 a, struct dsp_vec3 b)
{
	struct dsp_vec3 r = { a.x - b.x, a.y - b.y, a.z - b.z };
	return r;
}

static inline struct dsp_vec3 vec3_add(struct dsp_vec3 a, struct dsp_vec3 b)
{
	struct dsp_vec3 r = { a.x + b.x, a.y + b.y, a.z + b.z };
	return r;
}

static inline struct dsp_vec3 vec3_scale(struct dsp_vec3 a, float s)
{
	struct dsp_vec3 r = { a.x * s, a.y * s, a.z * s };
	return r;
}

static inline float vec3_dot(struct dsp_vec3 a, struct dsp_vec3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline struct dsp_vec3 vec3_cross(struct dsp_vec3 a, struct dsp_vec3 b)
{
	struct dsp_vec3 r = {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
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
	memset(scene, 0, sizeof(*scene));

	float x0 = -width * 0.5f, x1 = width * 0.5f;
	float y0 = 0.0f, y1 = height;
	float z0 = -length * 0.5f, z1 = length * 0.5f;

	struct dsp_vec3 v[8] = {
		{ x0, y0, z0 }, { x1, y0, z0 }, { x1, y0, z1 }, { x0, y0, z1 },
		{ x0, y1, z0 }, { x1, y1, z0 }, { x1, y1, z1 }, { x0, y1, z1 }
	};

	int tri_indices[12][3] = {
		{ 0, 1, 2 }, { 0, 2, 3 }, /* Floor */
		{ 4, 6, 5 }, { 4, 7, 6 }, /* Ceiling */
		{ 0, 5, 1 }, { 0, 4, 5 }, /* Back wall */
		{ 3, 2, 6 }, { 3, 6, 7 }, /* Front wall */
		{ 0, 3, 7 }, { 0, 7, 4 }, /* Left wall */
		{ 1, 5, 6 }, { 1, 6, 2 }  /* Right wall */
	};

	scene->num_triangles = 12;
	for (uint32_t i = 0; i < 12; i++) {
		scene->triangles[i].v0 = v[tri_indices[i][0]];
		scene->triangles[i].v1 = v[tri_indices[i][1]];
		scene->triangles[i].v2 = v[tri_indices[i][2]];

		struct dsp_vec3 e1 = vec3_sub(scene->triangles[i].v1, scene->triangles[i].v0);
		struct dsp_vec3 e2 = vec3_sub(scene->triangles[i].v2, scene->triangles[i].v0);
		scene->triangles[i].normal = vec3_normalize(vec3_cross(e1, e2));
		scene->triangles[i].absorption[0] = 0.1f;
		scene->triangles[i].absorption[1] = 0.15f;
		scene->triangles[i].absorption[2] = 0.2f;
	}

	/* Simple root node enclosing entire scene */
	scene->num_nodes = 1;
	scene->nodes[0].bounds.min = (struct dsp_vec3){ x0, y0, z0 };
	scene->nodes[0].bounds.max = (struct dsp_vec3){ x1, y1, z1 };
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
		return 1.0f; /* 100% occluded */

	return 0.0f;
}
