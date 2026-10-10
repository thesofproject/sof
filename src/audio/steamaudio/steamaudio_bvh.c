// SPDX-License-Identifier: Apache-2.0
//
// Copyright (c) 2017-2024 Valve Corporation. All rights reserved.
// Copyright (c) 2026 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Steam Audio Lightweight DSP BVH Ray Tracer

#include "steamaudio.h"
#include <sof/compiler_attributes.h>
#include <rtos/string.h>
#include <rtos/timer.h>
#include <math.h>

#define EPSILON 1e-6f

static inline __maybe_unused struct dsp_vec3 vec3_sub(struct dsp_vec3 a, struct dsp_vec3 b)
{
	struct dsp_vec3 r = { a.x - b.x, a.y - b.y, a.z - b.z };
	return r;
}

static inline __maybe_unused struct dsp_vec3 vec3_add(struct dsp_vec3 a, struct dsp_vec3 b)
{
	struct dsp_vec3 r = { a.x + b.x, a.y + b.y, a.z + b.z };
	return r;
}

static inline __maybe_unused struct dsp_vec3 vec3_scale(struct dsp_vec3 a, float s)
{
	struct dsp_vec3 r = { a.x * s, a.y * s, a.z * s };
	return r;
}

static inline __maybe_unused float vec3_dot(struct dsp_vec3 a, struct dsp_vec3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline __maybe_unused struct dsp_vec3 vec3_cross(struct dsp_vec3 a, struct dsp_vec3 b)
{
	struct dsp_vec3 r = {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
	return r;
}

static inline __maybe_unused struct dsp_vec3 vec3_normalize(struct dsp_vec3 a)
{
	float d2 = vec3_dot(a, a);
	if (d2 > 1e-9f) {
		float inv_len = fast_inv_sqrt(d2);
		return vec3_scale(a, inv_len);
	}
	return a;
}

static inline __maybe_unused float fminf_local(float a, float b) { return (a < b) ? a : b; }
static inline __maybe_unused float fmaxf_local(float a, float b) { return (a > b) ? a : b; }

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
	scene->nodes[0].left_child = -1;
	scene->nodes[0].right_child = -1;
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

void steamaudio_dsp_dynamic_geom_init(struct dsp_dynamic_geometry *dg)
{
	if (dg)
		memset(dg, 0, sizeof(*dg));
}

int steamaudio_dsp_dynamic_geom_stream(struct dsp_dynamic_geometry *dg,
				       const struct sof_steamaudio_geom_stream_payload *stream)
{
	if (!dg || !stream)
		return -EINVAL;

	uint32_t op = stream->header.op;
	uint32_t target_mesh = stream->header.mesh_id;
	uint32_t count = stream->header.num_triangles;
	if (count > 32)
		count = 32;

	if (op == STEAMAUDIO_GEOM_OP_CLEAR) {
		dg->num_triangles = 0;
		dg->ring_write_seq = stream->header.ring_write_seq;
		return 0;
	}

	if (op == STEAMAUDIO_GEOM_OP_REMOVE_MESH || op == STEAMAUDIO_GEOM_OP_UPDATE_MESH) {
		/* Compact array to remove existing triangles belonging to target_mesh */
		uint32_t write_idx = 0;
		for (uint32_t i = 0; i < dg->num_triangles; i++) {
			if (dg->mesh_ids[i] != target_mesh) {
				if (write_idx != i) {
					dg->triangles[write_idx] = dg->triangles[i];
					dg->transmission[write_idx][0] = dg->transmission[i][0];
					dg->transmission[write_idx][1] = dg->transmission[i][1];
					dg->transmission[write_idx][2] = dg->transmission[i][2];
					dg->mesh_ids[write_idx] = dg->mesh_ids[i];
				}
				write_idx++;
			}
		}
		dg->num_triangles = write_idx;
		if (op == STEAMAUDIO_GEOM_OP_REMOVE_MESH) {
			dg->ring_write_seq = stream->header.ring_write_seq;
			return 0;
		}
	}

	/* Append or insert new triangles */
	for (uint32_t i = 0; i < count; i++) {
		if (dg->num_triangles >= STEAMAUDIO_MAX_DYNAMIC_TRIANGLES)
			break;

		uint32_t idx = dg->num_triangles;
		const struct sof_steamaudio_triangle *st = &stream->triangles[i];

		dg->triangles[idx].v0 = (struct dsp_vec3){ st->v0[0], st->v0[1], st->v0[2] };
		dg->triangles[idx].v1 = (struct dsp_vec3){ st->v1[0], st->v1[1], st->v1[2] };
		dg->triangles[idx].v2 = (struct dsp_vec3){ st->v2[0], st->v2[1], st->v2[2] };

		/* Compute normal if zero */
		float nlen2 = st->normal[0] * st->normal[0] + st->normal[1] * st->normal[1] + st->normal[2] * st->normal[2];
		if (nlen2 > 1e-6f) {
			dg->triangles[idx].normal = (struct dsp_vec3){ st->normal[0], st->normal[1], st->normal[2] };
		} else {
			struct dsp_vec3 e1 = vec3_sub(dg->triangles[idx].v1, dg->triangles[idx].v0);
			struct dsp_vec3 e2 = vec3_sub(dg->triangles[idx].v2, dg->triangles[idx].v0);
			dg->triangles[idx].normal = vec3_normalize(vec3_cross(e1, e2));
		}

		dg->transmission[idx][0] = st->transmission[0];
		dg->transmission[idx][1] = st->transmission[1];
		dg->transmission[idx][2] = st->transmission[2];
		dg->mesh_ids[idx] = (op == STEAMAUDIO_GEOM_OP_UPDATE_MESH) ? target_mesh : st->mesh_id;

		dg->num_triangles++;
	}

	dg->ring_write_seq = stream->header.ring_write_seq;
	return 0;
}

float steamaudio_dsp_test_dynamic_occlusion(const struct dsp_dynamic_geometry *dg,
					    struct dsp_vec3 source,
					    struct dsp_vec3 listener,
					    float out_transmission[3])
{
	if (out_transmission) {
		out_transmission[0] = 1.0f;
		out_transmission[1] = 1.0f;
		out_transmission[2] = 1.0f;
	}

	if (!dg || dg->num_triangles == 0)
		return 0.0f;

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

	bool has_hit = false;
	for (uint32_t i = 0; i < dg->num_triangles; i++) {
		struct dsp_hit hit;
		if (intersect_ray_triangle(&ray, &dg->triangles[i], &hit)) {
			has_hit = true;
			if (out_transmission) {
				out_transmission[0] *= dg->transmission[i][0];
				out_transmission[1] *= dg->transmission[i][1];
				out_transmission[2] *= dg->transmission[i][2];
			}
		}
	}

	if (has_hit) {
		if (out_transmission) {
			float min_trans = out_transmission[0];
			if (out_transmission[1] < min_trans) min_trans = out_transmission[1];
			if (out_transmission[2] < min_trans) min_trans = out_transmission[2];
			return 1.0f - min_trans;
		}
		return 1.0f;
	}

	return 0.0f;
}

/* ========================================================================= */
/* Hierarchical Instanced Mesh (TLAS / BLAS) Scene Graph Implementation      */
/* ========================================================================= */

static void dsp_mat4_identity(struct dsp_mat4 *mat)
{
	for (int i = 0; i < 16; i++)
		mat->m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

static struct dsp_vec3 dsp_mat4_mul_point(const struct dsp_mat4 *mat, struct dsp_vec3 p)
{
	const float *m = mat->m;
	float x = m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3];
	float y = m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7];
	float z = m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11];
	float w = m[12] * p.x + m[13] * p.y + m[14] * p.z + m[15];
	if (w != 1.0f && w != 0.0f) {
		float inv_w = 1.0f / w;
		x *= inv_w;
		y *= inv_w;
		z *= inv_w;
	}
	return (struct dsp_vec3){ x, y, z };
}

static inline __maybe_unused struct dsp_vec3 dsp_mat4_mul_dir(const struct dsp_mat4 *mat, struct dsp_vec3 d)
{
	const float *m = mat->m;
	float x = m[0] * d.x + m[1] * d.y + m[2] * d.z;
	float y = m[4] * d.x + m[5] * d.y + m[6] * d.z;
	float z = m[8] * d.x + m[9] * d.y + m[10] * d.z;
	return (struct dsp_vec3){ x, y, z };
}

static bool dsp_mat4_invert(const struct dsp_mat4 *in, struct dsp_mat4 *out)
{
	const float *m = in->m;
	float inv[16];

	inv[0] = m[5]  * m[10] * m[15] - m[5]  * m[11] * m[14] - 
             m[9]  * m[6]  * m[15] + m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] - m[13] * m[7]  * m[10];

	inv[4] = -m[4]  * m[10] * m[15] + m[4]  * m[11] * m[14] + 
              m[8]  * m[6]  * m[15] - m[8]  * m[7]  * m[14] - 
              m[12] * m[6]  * m[11] + m[12] * m[7]  * m[10];

	inv[8] = m[4]  * m[9] * m[15] - m[4]  * m[11] * m[13] - 
             m[8]  * m[5] * m[15] + m[8]  * m[7] * m[13] + 
             m[12] * m[5] * m[11] - m[12] * m[7] * m[9];

	inv[12] = -m[4]  * m[9] * m[14] + m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] - m[8]  * m[6] * m[13] - 
               m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

	inv[1] = -m[1]  * m[10] * m[15] + m[1]  * m[11] * m[14] + 
              m[9]  * m[2] * m[15] - m[9]  * m[3] * m[14] - 
              m[13] * m[2] * m[11] + m[13] * m[3] * m[10];

	inv[5] = m[0]  * m[10] * m[15] - m[0]  * m[11] * m[14] - 
             m[8]  * m[2] * m[15] + m[8]  * m[3] * m[14] + 
             m[12] * m[2] * m[11] - m[12] * m[3] * m[10];

	inv[9] = -m[0]  * m[9] * m[15] + m[0]  * m[11] * m[13] + 
              m[8]  * m[1] * m[15] - m[8]  * m[3] * m[13] - 
              m[12] * m[1] * m[11] + m[12] * m[3] * m[9];

	inv[13] = m[0]  * m[9] * m[14] - m[0]  * m[10] * m[13] - 
              m[8]  * m[1] * m[14] + m[8]  * m[2] * m[13] + 
              m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

	inv[2] = m[1]  * m[6] * m[15] - m[1]  * m[7] * m[14] - 
             m[5]  * m[2] * m[15] + m[5]  * m[3] * m[14] + 
             m[13] * m[2] * m[7] - m[13] * m[3] * m[6];

	inv[6] = -m[0]  * m[6] * m[15] + m[0]  * m[7] * m[14] + 
              m[4]  * m[2] * m[15] - m[4]  * m[3] * m[14] - 
              m[12] * m[2] * m[7] + m[12] * m[3] * m[6];

	inv[10] = m[0]  * m[5] * m[15] - m[0]  * m[7] * m[13] - 
              m[4]  * m[1] * m[15] + m[4]  * m[3] * m[13] + 
              m[12] * m[1] * m[7] - m[12] * m[3] * m[5];

	inv[14] = -m[0]  * m[5] * m[14] + m[0]  * m[6] * m[13] + 
               m[4]  * m[1] * m[14] - m[4]  * m[2] * m[13] - 
               m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

	inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + 
              m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - 
              m[9] * m[2] * m[7] + m[9] * m[3] * m[6];

	inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - 
             m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + 
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];

	inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + 
               m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - 
               m[8] * m[1] * m[7] + m[8] * m[3] * m[5];

	inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - 
              m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + 
              m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

	float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
	if (det > -1e-8f && det < 1e-8f) {
		dsp_mat4_identity(out);
		return false;
	}

	float inv_det = 1.0f / det;
	for (int i = 0; i < 16; i++)
		out->m[i] = inv[i] * inv_det;

	return true;
}

void steamaudio_dsp_instanced_mesh_init(struct dsp_instanced_mesh_state *ims)
{
	if (!ims)
		return;
	memset(ims, 0, sizeof(*ims));
	ims->enabled = true;
}

int steamaudio_dsp_instanced_mesh_set_prototype(struct dsp_instanced_mesh_state *ims,
						uint32_t prototype_id,
						const struct sof_steamaudio_triangle *triangles,
						uint32_t num_triangles)
{
	if (!ims)
		return -EINVAL;

	int proto_idx = -1;
	for (uint32_t i = 0; i < ims->num_prototypes; i++) {
		if (ims->prototypes[i].prototype_id == prototype_id) {
			proto_idx = (int)i;
			break;
		}
	}

	if (proto_idx < 0) {
		if (ims->num_prototypes >= STEAMAUDIO_MAX_BLAS_PROTOTYPES)
			return -ENOMEM;
		proto_idx = (int)ims->num_prototypes++;
	}

	struct dsp_blas_prototype *proto = &ims->prototypes[proto_idx];
	proto->prototype_id = prototype_id;
	if (num_triangles > STEAMAUDIO_MAX_BLAS_TRIANGLES)
		num_triangles = STEAMAUDIO_MAX_BLAS_TRIANGLES;
	proto->num_triangles = num_triangles;

	for (uint32_t i = 0; i < num_triangles; i++) {
		const struct sof_steamaudio_triangle *st = &triangles[i];
		proto->triangles[i].v0 = (struct dsp_vec3){ st->v0[0], st->v0[1], st->v0[2] };
		proto->triangles[i].v1 = (struct dsp_vec3){ st->v1[0], st->v1[1], st->v1[2] };
		proto->triangles[i].v2 = (struct dsp_vec3){ st->v2[0], st->v2[1], st->v2[2] };

		float nlen2 = st->normal[0] * st->normal[0] + st->normal[1] * st->normal[1] + st->normal[2] * st->normal[2];
		if (nlen2 > 1e-6f) {
			proto->triangles[i].normal = (struct dsp_vec3){ st->normal[0], st->normal[1], st->normal[2] };
		} else {
			struct dsp_vec3 e1 = vec3_sub(proto->triangles[i].v1, proto->triangles[i].v0);
			struct dsp_vec3 e2 = vec3_sub(proto->triangles[i].v2, proto->triangles[i].v0);
			proto->triangles[i].normal = vec3_normalize(vec3_cross(e1, e2));
		}

		proto->transmission[i][0] = st->transmission[0];
		proto->transmission[i][1] = st->transmission[1];
		proto->transmission[i][2] = st->transmission[2];
	}

	return 0;
}

int steamaudio_dsp_instanced_mesh_set_instance(struct dsp_instanced_mesh_state *ims,
					       uint32_t instance_id,
					       uint32_t prototype_id,
					       const float transform[16])
{
	if (!ims)
		return -EINVAL;

	int inst_idx = -1;
	for (uint32_t i = 0; i < ims->num_instances; i++) {
		if (ims->instances[i].instance_id == instance_id) {
			inst_idx = (int)i;
			break;
		}
	}

	if (inst_idx < 0) {
		if (ims->num_instances >= STEAMAUDIO_MAX_INSTANCES)
			return -ENOMEM;
		inst_idx = (int)ims->num_instances++;
	}

	struct dsp_instanced_mesh_instance *inst = &ims->instances[inst_idx];
	inst->instance_id = instance_id;
	inst->prototype_id = prototype_id;
	inst->enabled = true;
	inst->transmission[0] = 0.0f;
	inst->transmission[1] = 0.0f;
	inst->transmission[2] = 0.0f;

	if (transform) {
		memcpy(inst->transform.m, transform, 16 * sizeof(float));
		dsp_mat4_invert(&inst->transform, &inst->inv_transform);
	} else {
		dsp_mat4_identity(&inst->transform);
		dsp_mat4_identity(&inst->inv_transform);
	}

	return 0;
}

int steamaudio_dsp_instanced_mesh_update_transform(struct dsp_instanced_mesh_state *ims,
						   uint32_t instance_id,
						   const float transform[16])
{
	if (!ims || !transform)
		return -EINVAL;

	for (uint32_t i = 0; i < ims->num_instances; i++) {
		if (ims->instances[i].instance_id == instance_id) {
			memcpy(ims->instances[i].transform.m, transform, 16 * sizeof(float));
			dsp_mat4_invert(&ims->instances[i].transform, &ims->instances[i].inv_transform);
			return 0;
		}
	}

	return -ENOENT;
}

int steamaudio_dsp_instanced_mesh_remove_instance(struct dsp_instanced_mesh_state *ims,
						  uint32_t instance_id)
{
	if (!ims)
		return -EINVAL;

	for (uint32_t i = 0; i < ims->num_instances; i++) {
		if (ims->instances[i].instance_id == instance_id) {
			for (uint32_t j = i; j + 1 < ims->num_instances; j++) {
				ims->instances[j] = ims->instances[j + 1];
			}
			ims->num_instances--;
			return 0;
		}
	}

	return -ENOENT;
}

void steamaudio_dsp_instanced_mesh_clear(struct dsp_instanced_mesh_state *ims)
{
	if (!ims)
		return;
	ims->num_instances = 0;
	ims->num_prototypes = 0;
}

static const struct dsp_blas_prototype *find_prototype(const struct dsp_instanced_mesh_state *ims,
						       uint32_t prototype_id)
{
	for (uint32_t i = 0; i < ims->num_prototypes; i++) {
		if (ims->prototypes[i].prototype_id == prototype_id)
			return &ims->prototypes[i];
	}
	return NULL;
}

bool steamaudio_dsp_instanced_mesh_trace_ray(const struct dsp_instanced_mesh_state *ims,
					     const struct dsp_ray *ray,
					     struct dsp_hit *hit,
					     bool muted)
{
	if (!hit)
		return false;

	hit->has_hit = false;
	hit->distance = ray ? ray->max_distance : 1e9f;

	if (muted || !ims || !ray || ims->num_instances == 0)
		return false;

	for (uint32_t inst_i = 0; inst_i < ims->num_instances; inst_i++) {
		const struct dsp_instanced_mesh_instance *inst = &ims->instances[inst_i];
		if (!inst->enabled)
			continue;

		const struct dsp_blas_prototype *proto = find_prototype(ims, inst->prototype_id);
		if (!proto || proto->num_triangles == 0)
			continue;

		/* Inverse transform ray from world space to object space */
		struct dsp_vec3 origin_loc = dsp_mat4_mul_point(&inst->inv_transform, ray->origin);
		struct dsp_vec3 p_start = dsp_mat4_mul_point(&inst->inv_transform,
							    vec3_add(ray->origin, vec3_scale(ray->direction, ray->min_distance)));
		float min_dist_loc = vec3_dot(vec3_sub(p_start, origin_loc), vec3_sub(p_start, origin_loc));
		if (min_dist_loc > 1e-9f)
			min_dist_loc = min_dist_loc * fast_inv_sqrt(min_dist_loc);
		else
			min_dist_loc = 0.0f;

		struct dsp_vec3 p_end = dsp_mat4_mul_point(&inst->inv_transform,
							  vec3_add(ray->origin, vec3_scale(ray->direction, ray->max_distance)));
		float max_dist_loc = vec3_dot(vec3_sub(p_end, origin_loc), vec3_sub(p_end, origin_loc));
		if (max_dist_loc > 1e-9f)
			max_dist_loc = max_dist_loc * fast_inv_sqrt(max_dist_loc);
		else
			max_dist_loc = 1e9f;

		struct dsp_vec3 p_1 = dsp_mat4_mul_point(&inst->inv_transform,
							vec3_add(ray->origin, ray->direction));
		struct dsp_vec3 dir_loc = vec3_normalize(vec3_sub(p_1, origin_loc));

		struct dsp_ray ray_loc;
		ray_loc.origin = origin_loc;
		ray_loc.direction = dir_loc;
		ray_loc.min_distance = min_dist_loc;
		ray_loc.max_distance = max_dist_loc;

		for (uint32_t tri_i = 0; tri_i < proto->num_triangles; tri_i++) {
			struct dsp_hit tri_hit;
			if (intersect_ray_triangle(&ray_loc, &proto->triangles[tri_i], &tri_hit)) {
				/* Transform hit point back to world space to obtain world distance */
				struct dsp_vec3 hit_pt_loc = vec3_add(ray_loc.origin, vec3_scale(ray_loc.direction, tri_hit.distance));
				struct dsp_vec3 hit_pt_world = dsp_mat4_mul_point(&inst->transform, hit_pt_loc);
				struct dsp_vec3 to_hit = vec3_sub(hit_pt_world, ray->origin);
				float dist_world2 = vec3_dot(to_hit, to_hit);
				float dist_world = dist_world2 > 1e-9f ? dist_world2 * fast_inv_sqrt(dist_world2) : 0.0f;

				if (dist_world < hit->distance && dist_world >= ray->min_distance && dist_world <= ray->max_distance) {
					hit->has_hit = true;
					hit->distance = dist_world;
					hit->triangle_index = tri_i;

					/* Normal transform: N_world = normalize((M_inv)^T * N_local) */
					const float *inv_m = inst->inv_transform.m;
					float nx = inv_m[0] * tri_hit.normal.x + inv_m[4] * tri_hit.normal.y + inv_m[8] * tri_hit.normal.z;
					float ny = inv_m[1] * tri_hit.normal.x + inv_m[5] * tri_hit.normal.y + inv_m[9] * tri_hit.normal.z;
					float nz = inv_m[2] * tri_hit.normal.x + inv_m[6] * tri_hit.normal.y + inv_m[10] * tri_hit.normal.z;
					hit->normal = vec3_normalize((struct dsp_vec3){ nx, ny, nz });
				}
			}
		}
	}

	return hit->has_hit;
}

float steamaudio_dsp_instanced_mesh_test_occlusion(const struct dsp_instanced_mesh_state *ims,
						   struct dsp_vec3 source,
						   struct dsp_vec3 listener,
						   float out_transmission[3],
						   bool muted)
{
	if (out_transmission) {
		out_transmission[0] = 1.0f;
		out_transmission[1] = 1.0f;
		out_transmission[2] = 1.0f;
	}

	if (muted || !ims || ims->num_instances == 0)
		return 0.0f;

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

	bool has_hit = false;

	for (uint32_t inst_i = 0; inst_i < ims->num_instances; inst_i++) {
		const struct dsp_instanced_mesh_instance *inst = &ims->instances[inst_i];
		if (!inst->enabled)
			continue;

		const struct dsp_blas_prototype *proto = find_prototype(ims, inst->prototype_id);
		if (!proto || proto->num_triangles == 0)
			continue;

		struct dsp_vec3 origin_loc = dsp_mat4_mul_point(&inst->inv_transform, ray.origin);
		struct dsp_vec3 p_start = dsp_mat4_mul_point(&inst->inv_transform,
							    vec3_add(ray.origin, vec3_scale(ray.direction, ray.min_distance)));
		float min_dist_loc = vec3_dot(vec3_sub(p_start, origin_loc), vec3_sub(p_start, origin_loc));
		if (min_dist_loc > 1e-9f)
			min_dist_loc = min_dist_loc * fast_inv_sqrt(min_dist_loc);
		else
			min_dist_loc = 0.0f;

		struct dsp_vec3 p_end = dsp_mat4_mul_point(&inst->inv_transform,
							  vec3_add(ray.origin, vec3_scale(ray.direction, ray.max_distance)));
		float max_dist_loc = vec3_dot(vec3_sub(p_end, origin_loc), vec3_sub(p_end, origin_loc));
		if (max_dist_loc > 1e-9f)
			max_dist_loc = max_dist_loc * fast_inv_sqrt(max_dist_loc);
		else
			max_dist_loc = 1e9f;

		struct dsp_vec3 p_1 = dsp_mat4_mul_point(&inst->inv_transform,
							vec3_add(ray.origin, ray.direction));
		struct dsp_vec3 dir_loc = vec3_normalize(vec3_sub(p_1, origin_loc));

		struct dsp_ray ray_loc;
		ray_loc.origin = origin_loc;
		ray_loc.direction = dir_loc;
		ray_loc.min_distance = min_dist_loc;
		ray_loc.max_distance = max_dist_loc;

		for (uint32_t tri_i = 0; tri_i < proto->num_triangles; tri_i++) {
			struct dsp_hit tri_hit;
			if (intersect_ray_triangle(&ray_loc, &proto->triangles[tri_i], &tri_hit)) {
				has_hit = true;
				if (out_transmission) {
					out_transmission[0] *= proto->transmission[tri_i][0];
					out_transmission[1] *= proto->transmission[tri_i][1];
					out_transmission[2] *= proto->transmission[tri_i][2];
				}
			}
		}
	}

	if (has_hit) {
		if (out_transmission) {
			float min_trans = out_transmission[0];
			if (out_transmission[1] < min_trans) min_trans = out_transmission[1];
			if (out_transmission[2] < min_trans) min_trans = out_transmission[2];
			return 1.0f - min_trans;
		}
		return 1.0f;
	}

	return 0.0f;
}

/* --------------------------------------------------------------------------------------------------------------------
 * Multi-Bounce Acoustic Ray Tracer & Specular Path Simulation Implementation
 * --------------------------------------------------------------------------------------------------------------------
 */

void steamaudio_dsp_ray_tracer_init(struct dsp_ray_tracer_state *rts)
{
	if (!rts)
		return;

	memset(rts, 0, sizeof(*rts));
	rts->max_bounces = 4;
	rts->speed_of_sound = 343.0f;
	rts->irradiance_min_distance = 1.0f;
	rts->listener_radius = 1.0f;
	rts->default_material_absorption[0] = 0.10f;
	rts->default_material_absorption[1] = 0.10f;
	rts->default_material_absorption[2] = 0.10f;
	rts->scattering = 0.10f;
	rts->num_simulated_paths = 0;
	rts->enabled = true;
	rts->flags = 1;
}

void steamaudio_dsp_ray_tracer_set_config(struct dsp_ray_tracer_state *rts,
					 const struct sof_steamaudio_ray_tracer_config *cfg)
{
	if (!rts || !cfg)
		return;

	if (cfg->max_bounces > 0 && cfg->max_bounces <= STEAMAUDIO_RAY_TRACER_MAX_BOUNCES)
		rts->max_bounces = cfg->max_bounces;
	if (cfg->speed_of_sound > 10.0f)
		rts->speed_of_sound = cfg->speed_of_sound;
	if (cfg->irradiance_min_dist > 0.01f)
		rts->irradiance_min_distance = cfg->irradiance_min_dist;
	if (cfg->listener_radius > 0.01f)
		rts->listener_radius = cfg->listener_radius;

	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		if (cfg->material_absorption[b] >= 0.0f && cfg->material_absorption[b] <= 1.0f)
			rts->default_material_absorption[b] = cfg->material_absorption[b];
	}

	rts->scattering = cfg->scattering;
	rts->flags = cfg->flags;
	rts->enabled = (cfg->flags & 1) != 0;
}

static bool ray_tracer_find_closest_hit(const struct steamaudio_comp_data *cd,
					const struct dsp_ray *ray,
					struct dsp_hit *out_hit,
					float out_absorption[3],
					uint32_t *out_surface_type)
{
	out_hit->has_hit = false;
	out_hit->distance = 1e9f;

	if (out_absorption) {
		out_absorption[0] = 0.10f;
		out_absorption[1] = 0.10f;
		out_absorption[2] = 0.10f;
	}
	if (out_surface_type)
		*out_surface_type = 0;

	if (!cd)
		return false;

	/* 1. Test Static Scene */
	struct dsp_hit static_hit;
	static_hit.has_hit = false;
	static_hit.distance = 1e9f;
	if (steamaudio_dsp_trace_closest_hit(&cd->scene, ray, &static_hit)) {
		if (static_hit.has_hit && static_hit.distance < out_hit->distance &&
		    static_hit.distance >= ray->min_distance && static_hit.distance <= ray->max_distance) {
			*out_hit = static_hit;
			if (out_surface_type) *out_surface_type = 0;
			if (out_absorption) {
				out_absorption[0] = cd->ray_tracer.default_material_absorption[0];
				out_absorption[1] = cd->ray_tracer.default_material_absorption[1];
				out_absorption[2] = cd->ray_tracer.default_material_absorption[2];
			}
		}
	}

	/* 2. Test Dynamic Geometry */
	if (cd->dynamic_geom.num_triangles > 0) {
		for (uint32_t i = 0; i < cd->dynamic_geom.num_triangles; i++) {
			struct dsp_hit dyn_hit;
			if (intersect_ray_triangle(ray, &cd->dynamic_geom.triangles[i], &dyn_hit)) {
				if (dyn_hit.has_hit && dyn_hit.distance < out_hit->distance &&
				    dyn_hit.distance >= ray->min_distance && dyn_hit.distance <= ray->max_distance) {
					*out_hit = dyn_hit;
					if (out_surface_type) *out_surface_type = 1;
					if (out_absorption) {
						/* Surface absorption = 1.0 - transmission */
						out_absorption[0] = 1.0f - cd->dynamic_geom.transmission[i][0];
						out_absorption[1] = 1.0f - cd->dynamic_geom.transmission[i][1];
						out_absorption[2] = 1.0f - cd->dynamic_geom.transmission[i][2];
					}
				}
			}
		}
	}

	/* 3. Test Instanced Meshes */
	if (cd->instanced_mesh.num_instances > 0) {
		struct dsp_hit inst_hit;
		inst_hit.has_hit = false;
		inst_hit.distance = 1e9f;
		if (steamaudio_dsp_instanced_mesh_trace_ray(&cd->instanced_mesh, ray, &inst_hit, false)) {
			if (inst_hit.has_hit && inst_hit.distance < out_hit->distance &&
			    inst_hit.distance >= ray->min_distance && inst_hit.distance <= ray->max_distance) {
				*out_hit = inst_hit;
				if (out_surface_type) *out_surface_type = 2;
				if (out_absorption) {
					out_absorption[0] = cd->ray_tracer.default_material_absorption[0];
					out_absorption[1] = cd->ray_tracer.default_material_absorption[1];
					out_absorption[2] = cd->ray_tracer.default_material_absorption[2];
				}
			}
		}
	}

	return out_hit->has_hit;
}

void steamaudio_dsp_ray_tracer_trace_path(const struct steamaudio_comp_data *cd,
					 const struct dsp_ray_tracer_state *rts,
					 struct dsp_vec3 origin,
					 struct dsp_vec3 dir,
					 struct dsp_vec3 listener,
					 uint32_t ray_idx,
					 uint32_t max_bounces,
					 struct dsp_acoustic_ray_path *out_path)
{
	if (!out_path)
		return;

	memset(out_path, 0, sizeof(*out_path));
	out_path->ray_index = ray_idx;
	out_path->energy[0] = 1.0f;
	out_path->energy[1] = 1.0f;
	out_path->energy[2] = 1.0f;
	out_path->total_distance = 0.0f;
	out_path->delay_ms = 0.0f;
	out_path->reached_listener = 0;
	out_path->arrival_dir = dir;

	float speed = (rts && rts->speed_of_sound > 10.0f) ? rts->speed_of_sound : 343.0f;
	float lis_rad = (rts && rts->listener_radius > 0.01f) ? rts->listener_radius : 1.0f;
	uint32_t limit_bounces = (max_bounces > 0 && max_bounces <= STEAMAUDIO_RAY_TRACER_MAX_BOUNCES) ?
				  max_bounces : 4;

	struct dsp_vec3 cur_origin = origin;
	struct dsp_vec3 cur_dir = vec3_normalize(dir);

	for (uint32_t b = 0; b < limit_bounces; b++) {
		struct dsp_ray ray;
		ray.origin = cur_origin;
		ray.direction = cur_dir;
		ray.min_distance = 0.01f;
		ray.max_distance = 100.0f;

		struct dsp_hit hit = { 0 };
		float absorption[3];
		uint32_t surface_type = 0;

		bool has_hit = ray_tracer_find_closest_hit(cd, &ray, &hit, absorption, &surface_type);
		if (!has_hit) {
			/* Ray escaped into free space */
			break;
		}

		/* Record hit details */
		struct dsp_vec3 hit_pt = vec3_add(cur_origin, vec3_scale(cur_dir, hit.distance));
		struct dsp_vec3 norm = hit.normal;

		/* Orient normal against incident ray */
		if (vec3_dot(cur_dir, norm) > 0.0f)
			norm = vec3_scale(norm, -1.0f);

		struct dsp_ray_bounce_hit *bounce_rec = &out_path->bounces[out_path->num_bounces];
		bounce_rec->hit_point = hit_pt;
		bounce_rec->normal = norm;
		bounce_rec->distance = hit.distance;
		bounce_rec->absorption[0] = absorption[0];
		bounce_rec->absorption[1] = absorption[1];
		bounce_rec->absorption[2] = absorption[2];
		bounce_rec->surface_type = surface_type;

		out_path->total_distance += hit.distance;
		out_path->delay_ms = (out_path->total_distance / speed) * 1000.0f;

		/* Attenuate 3-band acoustic energy */
		out_path->energy[0] *= (1.0f - absorption[0]);
		out_path->energy[1] *= (1.0f - absorption[1]);
		out_path->energy[2] *= (1.0f - absorption[2]);
		if (out_path->energy[0] < 0.0f) out_path->energy[0] = 0.0f;
		if (out_path->energy[1] < 0.0f) out_path->energy[1] = 0.0f;
		if (out_path->energy[2] < 0.0f) out_path->energy[2] = 0.0f;

		out_path->num_bounces++;

		/* Check distance to listener along this segment */
		struct dsp_vec3 v_lis = vec3_sub(listener, cur_origin);
		float proj = vec3_dot(v_lis, cur_dir);
		if (proj > 0.0f && proj <= hit.distance) {
			struct dsp_vec3 closest = vec3_add(cur_origin, vec3_scale(cur_dir, proj));
			struct dsp_vec3 d_lis = vec3_sub(listener, closest);
			float d_lis2 = vec3_dot(d_lis, d_lis);
			if (d_lis2 <= lis_rad * lis_rad) {
				out_path->reached_listener = 1;
				out_path->arrival_dir = cur_dir;
			}
		}

		/* Compute Law of Reflection: r = d - 2(d . n) n */
		float d_dot_n = vec3_dot(cur_dir, norm);
		struct dsp_vec3 refl = vec3_sub(cur_dir, vec3_scale(norm, 2.0f * d_dot_n));
		refl = vec3_normalize(refl);

		/* Prepare next bounce origin with safety epsilon offset */
		cur_origin = vec3_add(hit_pt, vec3_scale(norm, 0.005f));
		cur_dir = refl;
		out_path->arrival_dir = refl;
	}
}

void steamaudio_dsp_ray_tracer_simulate_batch(const struct steamaudio_comp_data *cd,
					      struct dsp_ray_tracer_state *rts,
					      struct sof_steamaudio_ray_tracer_config *cfg,
					      bool muted)
{
	if (!cfg)
		return;

	uint32_t num_rays = cfg->num_rays;
	if (num_rays > STEAMAUDIO_RAY_TRACER_MAX_RAYS)
		num_rays = STEAMAUDIO_RAY_TRACER_MAX_RAYS;
	if (num_rays == 0)
		num_rays = 1;

	cfg->num_results = num_rays;

	if (muted) {
		/* Step 23 Muted: pass-through unreflected baseline */
		for (uint32_t i = 0; i < num_rays; i++) {
			cfg->results[i].num_bounces = 0;
			cfg->results[i].total_distance = 0.0f;
			cfg->results[i].delay_ms = 0.0f;
			cfg->results[i].energy[0] = 1.0f;
			cfg->results[i].energy[1] = 1.0f;
			cfg->results[i].energy[2] = 1.0f;
			cfg->results[i].arrival_dir[0] = cfg->ray_directions[i][0];
			cfg->results[i].arrival_dir[1] = cfg->ray_directions[i][1];
			cfg->results[i].arrival_dir[2] = cfg->ray_directions[i][2];
			cfg->results[i].reached_listener = 0;
		}
		return;
	}

	if (rts)
		steamaudio_dsp_ray_tracer_set_config(rts, cfg);

	struct dsp_vec3 src = { cfg->source_pos[0], cfg->source_pos[1], cfg->source_pos[2] };
	struct dsp_vec3 lis = { cfg->listener_pos[0], cfg->listener_pos[1], cfg->listener_pos[2] };
	uint32_t max_bounces = cfg->max_bounces ? cfg->max_bounces : 4;

	for (uint32_t i = 0; i < num_rays; i++) {
		struct dsp_vec3 dir = { cfg->ray_directions[i][0], cfg->ray_directions[i][1], cfg->ray_directions[i][2] };
		struct dsp_acoustic_ray_path path;

		steamaudio_dsp_ray_tracer_trace_path(cd, rts, src, dir, lis, i, max_bounces, &path);

		if (rts) {
			rts->paths[i] = path;
			rts->num_simulated_paths = num_rays;
		}

		cfg->results[i].num_bounces = path.num_bounces;
		cfg->results[i].total_distance = path.total_distance;
		cfg->results[i].delay_ms = path.delay_ms;
		cfg->results[i].energy[0] = path.energy[0];
		cfg->results[i].energy[1] = path.energy[1];
		cfg->results[i].energy[2] = path.energy[2];
		cfg->results[i].arrival_dir[0] = path.arrival_dir.x;
		cfg->results[i].arrival_dir[1] = path.arrival_dir.y;
		cfg->results[i].arrival_dir[2] = path.arrival_dir.z;
		cfg->results[i].reached_listener = path.reached_listener;
	}
}

void steamaudio_dsp_diffuse_reflect(struct dsp_vec3 in_dir,
				    struct dsp_vec3 normal,
				    float scattering,
				    float seed_u,
				    float seed_v,
				    struct dsp_vec3 *out_dir)
{
	if (!out_dir)
		return;

	/* Specular reflection: r = d - 2(d.n)n */
	float d_dot_n = vec3_dot(in_dir, normal);
	struct dsp_vec3 r_spec = vec3_sub(in_dir, vec3_scale(normal, 2.0f * d_dot_n));
	r_spec = vec3_normalize(r_spec);

	if (scattering <= 0.001f) {
		*out_dir = r_spec;
		return;
	}

	/* Build orthonormal basis around surface normal */
	struct dsp_vec3 up = (fabsf(normal.z) < 0.99f) ? (struct dsp_vec3){0.0f, 0.0f, 1.0f} : (struct dsp_vec3){1.0f, 0.0f, 0.0f};
	struct dsp_vec3 tangent = vec3_normalize(vec3_cross(up, normal));
	struct dsp_vec3 bitangent = vec3_cross(normal, tangent);

	/* Cosine-weighted hemisphere sampling */
	float phi = 2.0f * 3.14159265f * seed_u;
	float r = sqrtf(fmaxf(seed_v, 0.0f));
	float z = sqrtf(fmaxf(1.0f - seed_v, 0.0f));
	float x = r * cosf(phi);
	float y = r * sinf(phi);

	struct dsp_vec3 r_diff = vec3_add(vec3_add(vec3_scale(tangent, x), vec3_scale(bitangent, y)), vec3_scale(normal, z));
	r_diff = vec3_normalize(r_diff);

	if (vec3_dot(r_diff, normal) < 0.0f)
		r_diff = vec3_sub((struct dsp_vec3){0.0f, 0.0f, 0.0f}, r_diff);

	/* Blend specular and diffuse */
	float s = scattering;
	if (s > 1.0f) s = 1.0f;
	struct dsp_vec3 blended = vec3_add(vec3_scale(r_spec, 1.0f - s), vec3_scale(r_diff, s));
	*out_dir = vec3_normalize(blended);
}

void steamaudio_dsp_reverb_estimator_init(struct dsp_reverb_estimator_state *res)
{
	if (!res)
		return;

	memset(res, 0, sizeof(*res));
	res->num_bins = 100;
	res->bin_duration_s = 0.01f;
	res->early_cutoff_s = 0.08f;
	res->scattering = 0.2f;
	res->enabled = true;
	res->flags = 1;

	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		res->results.rt60[b] = 1.0f;
		for (uint32_t i = 0; i < STEAMAUDIO_REVERB_ESTIMATOR_MAX_BINS; i++)
			res->results.edc[b][i] = 0.0f;
	}
}

void steamaudio_dsp_reverb_estimator_set_config(struct dsp_reverb_estimator_state *res,
						const struct sof_steamaudio_reverb_estimator_config *cfg)
{
	if (!res || !cfg)
		return;

	if (cfg->num_bins > 0 && cfg->num_bins <= STEAMAUDIO_REVERB_ESTIMATOR_MAX_BINS)
		res->num_bins = cfg->num_bins;
	if (cfg->bin_duration_s > 0.0001f)
		res->bin_duration_s = cfg->bin_duration_s;
	if (cfg->early_cutoff_s > 0.001f)
		res->early_cutoff_s = cfg->early_cutoff_s;
	if (cfg->scattering >= 0.0f)
		res->scattering = cfg->scattering;

	res->flags = cfg->flags;
	res->enabled = (cfg->flags & 1) != 0;
}

void steamaudio_dsp_reverb_estimator_accumulate_rays(struct dsp_reverb_estimator_state *res,
						     const struct dsp_acoustic_ray_path *paths,
						     uint32_t num_paths)
{
	if (!res || !paths || num_paths == 0)
		return;

	float dt = res->bin_duration_s > 0.0001f ? res->bin_duration_s : 0.01f;
	uint32_t max_b = res->num_bins;
	if (max_b > STEAMAUDIO_REVERB_ESTIMATOR_MAX_BINS)
		max_b = STEAMAUDIO_REVERB_ESTIMATOR_MAX_BINS;

	for (uint32_t p = 0; p < num_paths; p++) {
		float delay_s = paths[p].delay_ms / 1000.0f;
		if (delay_s < 0.0f)
			delay_s = 0.0f;

		uint32_t bin_idx = (uint32_t)(delay_s / dt);
		if (bin_idx < max_b) {
			res->histogram.bins[0][bin_idx] += paths[p].energy[0];
			res->histogram.bins[1][bin_idx] += paths[p].energy[1];
			res->histogram.bins[2][bin_idx] += paths[p].energy[2];
		}
	}
}

void steamaudio_dsp_reverb_estimator_compute_edc_rt60(struct dsp_reverb_estimator_state *res,
						      bool muted)
{
	if (!res)
		return;

	if (muted) {
		/* Step 24 Muted: Zeroed/bypassed reverberation parameters */
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
			res->results.rt60[b] = 0.0f;
			res->results.early_energy[b] = 0.0f;
			res->results.late_energy[b] = 0.0f;
			res->results.total_energy[b] = 0.0f;
			for (uint32_t i = 0; i < STEAMAUDIO_REVERB_ESTIMATOR_MAX_BINS; i++)
				res->results.edc[b][i] = -100.0f;
		}
		res->results.direct_delay_ms = 0.0f;
		res->results.late_delay_ms = 0.0f;
		return;
	}

	uint32_t num_bins = res->num_bins;
	if (num_bins > STEAMAUDIO_REVERB_ESTIMATOR_MAX_BINS)
		num_bins = STEAMAUDIO_REVERB_ESTIMATOR_MAX_BINS;
	if (num_bins == 0)
		num_bins = 100;

	float dt = res->bin_duration_s > 0.0001f ? res->bin_duration_s : 0.01f;
	float early_cutoff = res->early_cutoff_s > 0.001f ? res->early_cutoff_s : 0.08f;
	uint32_t early_cutoff_bin = (uint32_t)(early_cutoff / dt);

	/* Direct delay: time of first non-zero bin in any band */
	int first_bin = -1;
	for (uint32_t i = 0; i < num_bins; i++) {
		if (res->histogram.bins[0][i] > 1e-6f ||
		    res->histogram.bins[1][i] > 1e-6f ||
		    res->histogram.bins[2][i] > 1e-6f) {
			first_bin = (int)i;
			break;
		}
	}
	res->results.direct_delay_ms = (first_bin >= 0) ? (first_bin * dt * 1000.0f) : 0.0f;
	res->results.late_delay_ms = (early_cutoff * 1000.0f) - res->results.direct_delay_ms;
	if (res->results.late_delay_ms < 0.0f)
		res->results.late_delay_ms = 0.0f;

	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		/* Schroeder backward integration: EDC[i] = sum_{j=i}^{num_bins-1} bins[b][j] */
		float edc_raw[STEAMAUDIO_REVERB_ESTIMATOR_MAX_BINS];
		float accum = 0.0f;
		for (int i = (int)num_bins - 1; i >= 0; i--) {
			accum += res->histogram.bins[b][i];
			edc_raw[i] = accum;
		}

		float total_energy = edc_raw[0];
		res->results.total_energy[b] = total_energy;

		/* Early vs Late energy */
		float early_energy = 0.0f;
		float late_energy = 0.0f;
		for (uint32_t i = 0; i < num_bins; i++) {
			if (i < early_cutoff_bin)
				early_energy += res->histogram.bins[b][i];
			else
				late_energy += res->histogram.bins[b][i];
		}
		res->results.early_energy[b] = early_energy;
		res->results.late_energy[b] = late_energy;

		if (total_energy <= 1e-7f) {
			res->results.rt60[b] = 0.1f;
			for (uint32_t i = 0; i < num_bins; i++)
				res->results.edc[b][i] = -100.0f;
			continue;
		}

		/* Convert to normalized decibels: 10 * log10(EDC[i] / total_energy) */
		for (uint32_t i = 0; i < num_bins; i++) {
			float norm = edc_raw[i] / total_energy;
			if (norm < 1e-10f)
				norm = 1e-10f;
			res->results.edc[b][i] = 10.0f * log10f(norm);
		}

		/* Linear least-squares regression over decay curve */
		/* Fit line: y = m * t + c */
		float sum_t = 0.0f;
		float sum_y = 0.0f;
		float sum_t2 = 0.0f;
		float sum_ty = 0.0f;
		int n_pts = 0;

		for (uint32_t i = 0; i < num_bins; i++) {
			float y = res->results.edc[b][i];
			/* Fit window: between -5 dB and -25 dB */
			if (y <= -5.0f && y >= -25.0f) {
				float t = i * dt;
				sum_t += t;
				sum_y += y;
				sum_t2 += t * t;
				sum_ty += t * y;
				n_pts++;
			}
		}

		/* Fallback if range didn't capture enough points: use full decay curve */
		if (n_pts < 3) {
			sum_t = 0.0f; sum_y = 0.0f; sum_t2 = 0.0f; sum_ty = 0.0f; n_pts = 0;
			for (uint32_t i = 0; i < num_bins; i++) {
				float y = res->results.edc[b][i];
				if (y <= -1.0f && y >= -40.0f) {
					float t = i * dt;
					sum_t += t;
					sum_y += y;
					sum_t2 += t * t;
					sum_ty += t * y;
					n_pts++;
				}
			}
		}

		float rt60 = 1.0f;
		if (n_pts >= 2) {
			float denom = ((float)n_pts * sum_t2) - (sum_t * sum_t);
			if (fabsf(denom) > 1e-7f) {
				float slope = (((float)n_pts * sum_ty) - (sum_t * sum_y)) / denom;
				if (slope < -0.1f) {
					rt60 = -60.0f / slope;
				}
			}
		}

		/* Clamp to valid acoustic range [0.1s, 10.0s] */
		if (rt60 < 0.1f) rt60 = 0.1f;
		if (rt60 > 10.0f) rt60 = 10.0f;
		res->results.rt60[b] = rt60;
	}
}

/* Early Reflections Synthesizer & Tapped-Delay Filter Bank Implementation */

void steamaudio_dsp_early_reflections_init(struct dsp_early_reflections_state *ers)
{
	if (!ers)
		return;

	memset(ers, 0, sizeof(*ers));
	ers->sample_rate = 48000.0f;
	ers->speed_of_sound = 343.0f;
	ers->num_channels = 2;
	ers->enabled = true;
	ers->flags = 1;
}

void steamaudio_dsp_early_reflections_reset(struct dsp_early_reflections_state *ers)
{
	if (!ers)
		return;

	memset(ers->delay_line, 0, sizeof(ers->delay_line));
	ers->write_pos = 0;
}

void steamaudio_dsp_early_reflections_set_config(struct dsp_early_reflections_state *ers,
						const struct sof_steamaudio_early_reflections_config *cfg)
{
	if (!ers || !cfg)
		return;

	uint32_t nt = cfg->num_taps;
	if (nt > STEAMAUDIO_EARLY_REFLECTIONS_MAX_TAPS)
		nt = STEAMAUDIO_EARLY_REFLECTIONS_MAX_TAPS;
	ers->num_taps = nt;

	if (cfg->sample_rate > 0.0f)
		ers->sample_rate = cfg->sample_rate;
	if (cfg->speed_of_sound > 0.0f)
		ers->speed_of_sound = cfg->speed_of_sound;

	ers->num_channels = cfg->num_channels ? cfg->num_channels : 2;
	ers->enabled = (cfg->flags & 1) != 0;
	ers->flags = cfg->flags;

	for (uint32_t i = 0; i < nt; i++) {
		ers->taps[i] = cfg->taps[i];
	}
}

void steamaudio_dsp_early_reflections_set_taps_from_paths(struct dsp_early_reflections_state *ers,
							  const struct dsp_acoustic_ray_path *paths,
							  uint32_t num_paths,
							  float speed_of_sound)
{
	if (!ers || !paths)
		return;

	if (speed_of_sound <= 0.0f)
		speed_of_sound = ers->speed_of_sound > 0.0f ? ers->speed_of_sound : 343.0f;

	uint32_t tap_idx = 0;
	for (uint32_t i = 0; i < num_paths && tap_idx < STEAMAUDIO_EARLY_REFLECTIONS_MAX_TAPS; i++) {
		if (!paths[i].reached_listener)
			continue;

		float dist = paths[i].total_distance;
		float delay_s = dist / speed_of_sound;
		float delay_ms = delay_s * 1000.0f;

		/* Only arrivals within early reflection window (<= 80ms) */
		if (delay_ms > 80.0f)
			continue;

		ers->taps[tap_idx].delay_ms = delay_ms;
		ers->taps[tap_idx].direction[0] = paths[i].arrival_dir.x;
		ers->taps[tap_idx].direction[1] = paths[i].arrival_dir.y;
		ers->taps[tap_idx].direction[2] = paths[i].arrival_dir.z;

		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
			float e = paths[i].energy[b];
			if (e < 0.0f) e = 0.0f;
			ers->taps[tap_idx].gain[b] = sqrtf(e);
		}
		ers->taps[tap_idx].active = 1;
		tap_idx++;
	}

	ers->num_taps = tap_idx;
}

void steamaudio_dsp_early_reflections_process(struct dsp_early_reflections_state *ers,
					     const float *in,
					     float out[STEAMAUDIO_MAX_MIXER_CHANNELS][256],
					     uint32_t frames,
					     bool muted)
{
	if (!ers || !out)
		return;

	uint32_t channels = ers->num_channels;
	if (channels > STEAMAUDIO_MAX_MIXER_CHANNELS)
		channels = STEAMAUDIO_MAX_MIXER_CHANNELS;
	if (channels == 0)
		channels = 2;

	if (frames > 256)
		frames = 256;

	/* Bit-exact Step 25 Mute bypass: silence wet early reflections */
	if (muted || !ers->enabled) {
		for (uint32_t ch = 0; ch < channels; ch++) {
			memset(out[ch], 0, frames * sizeof(float));
		}
		return;
	}

	for (uint32_t ch = 0; ch < channels; ch++) {
		memset(out[ch], 0, frames * sizeof(float));
	}

	if (!in || ers->num_taps == 0)
		return;

	const uint32_t mask = STEAMAUDIO_EARLY_REFLECTIONS_DELAY_LINE_SIZE - 1;
	float fs = ers->sample_rate > 0.0f ? ers->sample_rate : 48000.0f;

	for (uint32_t i = 0; i < frames; i++) {
		/* Write current frame sample into circular buffer */
		ers->delay_line[ers->write_pos] = in[i];

		/* Synthesize all active early reflection taps */
		for (uint32_t t = 0; t < ers->num_taps; t++) {
			if (!ers->taps[t].active)
				continue;

			float d = ers->taps[t].delay_ms * 0.001f * fs;
			if (d < 0.0f)
				d = 0.0f;
			if (d > (float)(STEAMAUDIO_EARLY_REFLECTIONS_DELAY_LINE_SIZE - 2))
				d = (float)(STEAMAUDIO_EARLY_REFLECTIONS_DELAY_LINE_SIZE - 2);

			uint32_t k = (uint32_t)d;
			float frac = d - (float)k;

			uint32_t idx0 = (ers->write_pos + STEAMAUDIO_EARLY_REFLECTIONS_DELAY_LINE_SIZE - k) & mask;
			uint32_t idx1 = (ers->write_pos + STEAMAUDIO_EARLY_REFLECTIONS_DELAY_LINE_SIZE - k - 1) & mask;

			/* Fractional delay linear interpolation */
			float sample_val = (1.0f - frac) * ers->delay_line[idx0] + frac * ers->delay_line[idx1];

			/* 3-band absorption EQ gain weighting */
			float eq_gain = (ers->taps[t].gain[0] + ers->taps[t].gain[1] + ers->taps[t].gain[2]) / 3.0f;
			float tap_signal = sample_val * eq_gain;

			/* Constant-power spatial panning based on incident direction vector x */
			float x = ers->taps[t].direction[0];
			float gL, gR;
			if (x <= -0.9999f) {
				gL = 1.0f;
				gR = 0.0f;
			} else if (x >= 0.9999f) {
				gL = 0.0f;
				gR = 1.0f;
			} else {
				float theta = 0.785398163f * (1.0f + x); /* [0, pi/2] */
				gL = cosf(theta);
				gR = sinf(theta);
			}

			out[0][i] += tap_signal * gL;
			if (channels > 1) {
				out[1][i] += tap_signal * gR;
			}
		}

		ers->write_pos = (ers->write_pos + 1) & mask;
	}
}

/* ------------------------------------------------------------------------------------------------
 * Phase 38: Acoustic Material Transmission & Sound Wall Partitioning DSP
 * ------------------------------------------------------------------------------------------------ */

void steamaudio_dsp_material_transmission_init(struct dsp_material_transmission_state *mts)
{
	if (!mts)
		return;

	memset(mts, 0, sizeof(*mts));
	mts->composite_transmission[0] = 1.0f;
	mts->composite_transmission[1] = 1.0f;
	mts->composite_transmission[2] = 1.0f;
	mts->sample_rate = 48000;
	mts->enabled = true;
	mts->flags = 1;
}

void steamaudio_dsp_material_calculate_mass_law(float surface_density,
					       float out_transmission[STEAMAUDIO_NUM_EQ_BANDS])
{
	if (!out_transmission)
		return;

	if (surface_density <= 0.01f) {
		out_transmission[0] = 1.0f;
		out_transmission[1] = 1.0f;
		out_transmission[2] = 1.0f;
		return;
	}

	/* Octave band center frequencies: Low=400Hz, Mid=2500Hz, High=8000Hz */
	static const float freqs[STEAMAUDIO_NUM_EQ_BANDS] = { 400.0f, 2500.0f, 8000.0f };

	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		float mf = surface_density * freqs[b];
		if (mf < 1.0f)
			mf = 1.0f;

		/* Diffuse field mass law: TL = 20 * log10(m * f) - 52 dB */
		float tl = 20.0f * log10f(mf) - 52.0f;
		if (tl < 0.0f)
			tl = 0.0f;

		/* Transmission coefficient: T = 10^(-TL / 20) */
		float t = powf(10.0f, -tl / 20.0f);
		if (t > 1.0f)
			t = 1.0f;
		if (t < 0.0001f)
			t = 0.0001f;

		out_transmission[b] = t;
	}
}

void steamaudio_dsp_material_calculate_composite(const struct dsp_material_properties *layers,
						uint32_t num_layers,
						bool double_sided,
						float out_composite[STEAMAUDIO_NUM_EQ_BANDS])
{
	if (!out_composite)
		return;

	if (!layers || num_layers == 0) {
		out_composite[0] = 1.0f;
		out_composite[1] = 1.0f;
		out_composite[2] = 1.0f;
		return;
	}

	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		float prod = 1.0f;
		for (uint32_t k = 0; k < num_layers && k < STEAMAUDIO_MAX_MATERIAL_LAYERS; k++) {
			float val = layers[k].transmission[b];
			if (val < 0.0f)
				val = 0.0f;
			if (val > 1.0f)
				val = 1.0f;
			prod *= val;
		}

		/* Double-sided surface compensation (e.g. solid 3D wall hitting front and back faces) */
		if (double_sided && num_layers >= 2) {
			prod = sqrtf(prod);
		}

		if (prod > 1.0f)
			prod = 1.0f;
		if (prod < 0.00001f)
			prod = 0.00001f;

		out_composite[b] = prod;
	}
}

void steamaudio_dsp_material_transmission_set_config(struct dsp_material_transmission_state *mts,
						    const struct sof_steamaudio_material_transmission_config *cfg)
{
	if (!mts || !cfg)
		return;

	uint32_t n = cfg->num_layers;
	if (n > STEAMAUDIO_MAX_MATERIAL_LAYERS)
		n = STEAMAUDIO_MAX_MATERIAL_LAYERS;

	mts->num_layers = n;
	mts->enabled = (cfg->flags & 1) != 0;
	mts->flags = cfg->flags;
	mts->sample_rate = cfg->sample_rate ? cfg->sample_rate : 48000;

	for (uint32_t k = 0; k < n; k++) {
		mts->layers[k].surface_density = cfg->layers[k].surface_density;
		mts->layers[k].thickness = cfg->layers[k].thickness;
		mts->layers[k].preset = cfg->layers[k].preset;

		/* Apply preset if specified */
		switch (cfg->layers[k].preset) {
		case STEAMAUDIO_MATERIAL_DRYWALL:
			mts->layers[k].transmission[0] = 0.20f;
			mts->layers[k].transmission[1] = 0.08f;
			mts->layers[k].transmission[2] = 0.02f;
			break;
		case STEAMAUDIO_MATERIAL_WOOD:
			mts->layers[k].transmission[0] = 0.15f;
			mts->layers[k].transmission[1] = 0.05f;
			mts->layers[k].transmission[2] = 0.015f;
			break;
		case STEAMAUDIO_MATERIAL_GLASS:
			mts->layers[k].transmission[0] = 0.12f;
			mts->layers[k].transmission[1] = 0.04f;
			mts->layers[k].transmission[2] = 0.01f;
			break;
		case STEAMAUDIO_MATERIAL_CONCRETE:
			mts->layers[k].transmission[0] = 0.03f;
			mts->layers[k].transmission[1] = 0.008f;
			mts->layers[k].transmission[2] = 0.001f;
			break;
		case STEAMAUDIO_MATERIAL_METAL:
			mts->layers[k].transmission[0] = 0.08f;
			mts->layers[k].transmission[1] = 0.025f;
			mts->layers[k].transmission[2] = 0.005f;
			break;
		case STEAMAUDIO_MATERIAL_FABRIC:
			mts->layers[k].transmission[0] = 0.80f;
			mts->layers[k].transmission[1] = 0.60f;
			mts->layers[k].transmission[2] = 0.35f;
			break;
		default:
			if (cfg->flags & (1 << 1)) {
				/* Calculate from mass law */
				steamaudio_dsp_material_calculate_mass_law(
					cfg->layers[k].surface_density,
					mts->layers[k].transmission);
			} else {
				/* Direct user-specified transmission */
				for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
					mts->layers[k].transmission[b] = cfg->layers[k].transmission[b];
				}
			}
			break;
		}
	}

	/* Compute composite transmission across all wall layers */
	bool double_sided = (cfg->double_sided_compensation != 0);
	steamaudio_dsp_material_calculate_composite(mts->layers, n, double_sided,
						   mts->composite_transmission);
}

void steamaudio_dsp_material_transmission_process(struct dsp_material_transmission_state *mts,
						 const float *in,
						 float *out,
						 uint32_t frames,
						 bool muted)
{
	if (!in || !out)
		return;

	if (frames > 256)
		frames = 256;

	/* Bit-exact Step 26 Mute bypass: exact pass-through (out == in) */
	if (muted || !mts || !mts->enabled) {
		memcpy(out, in, frames * sizeof(float));
		return;
	}

	float fs = mts->sample_rate > 0 ? (float)mts->sample_rate : 48000.0f;

	/* 3-band crossover parameters: Low/Mid split at 800Hz, Mid/High split at 4000Hz */
	float w1 = 2.0f * 3.14159265f * 800.0f / fs;
	float a1 = w1 / (1.0f + w1);

	float w2 = 2.0f * 3.14159265f * 4000.0f / fs;
	float a2 = 1.0f / (1.0f + w2);

	float g0 = mts->composite_transmission[0];
	float g1 = mts->composite_transmission[1];
	float g2 = mts->composite_transmission[2];

	for (uint32_t i = 0; i < frames; i++) {
		float x = in[i];

		/* 1st-order low-pass filter state */
		mts->filter_states[0] += a1 * (x - mts->filter_states[0]);
		float x_low = mts->filter_states[0];

		/* 1st-order high-pass filter state */
		mts->filter_states[1] = a2 * (mts->filter_states[1] + x - mts->prev_input);
		mts->prev_input = x;
		float x_high = mts->filter_states[1];

		/* Mid-band is the complementary remainder for exact unity sum */
		float x_mid = x - x_low - x_high;

		/* Recombine with 3-band transmission coefficients */
		out[i] = g0 * x_low + g1 * x_mid + g2 * x_high;
	}
}

/* Acoustic Portals & Coupled Room-to-Room Energy Transfer Implementation */

void steamaudio_dsp_acoustic_portals_init(struct dsp_acoustic_portals_state *aps)
{
	if (!aps)
		return;

	memset(aps, 0, sizeof(*aps));
	aps->sample_rate = 48000;
	aps->enabled = true;
	aps->flags = 1;
}

void steamaudio_dsp_acoustic_portals_set_config(struct dsp_acoustic_portals_state *aps,
					       const struct sof_steamaudio_acoustic_portals_config *cfg)
{
	if (!aps || !cfg)
		return;

	uint32_t n = cfg->num_portals;
	if (n > STEAMAUDIO_MAX_PORTALS)
		n = STEAMAUDIO_MAX_PORTALS;

	aps->num_portals = n;
	aps->enabled = (cfg->flags & 1) != 0;
	aps->flags = cfg->flags;
	aps->sample_rate = cfg->sample_rate ? cfg->sample_rate : 48000;

	for (uint32_t k = 0; k < n; k++) {
		aps->portals[k].center[0] = cfg->portals[k].center[0];
		aps->portals[k].center[1] = cfg->portals[k].center[1];
		aps->portals[k].center[2] = cfg->portals[k].center[2];

		aps->portals[k].normal[0] = cfg->portals[k].normal[0];
		aps->portals[k].normal[1] = cfg->portals[k].normal[1];
		aps->portals[k].normal[2] = cfg->portals[k].normal[2];

		aps->portals[k].dimensions[0] = cfg->portals[k].dimensions[0];
		aps->portals[k].dimensions[1] = cfg->portals[k].dimensions[1];
		aps->portals[k].area = cfg->portals[k].area;
		aps->portals[k].openness = cfg->portals[k].openness;
		aps->portals[k].room_id_front = cfg->portals[k].room_id_front;
		aps->portals[k].room_id_back = cfg->portals[k].room_id_back;
		aps->portals[k].enabled = cfg->portals[k].enabled;
	}
}

void steamaudio_dsp_acoustic_portals_evaluate_coupling(const struct dsp_acoustic_portals_state *aps,
						     struct dsp_vec3 source,
						     struct dsp_vec3 listener,
						     int32_t src_room,
						     int32_t lis_room,
						     struct dsp_portal_coupling_result *out_res)
{
	if (!out_res)
		return;

	struct dsp_vec3 dir_sl = vec3_sub(listener, source);
	float direct_dist = sqrtf(vec3_dot(dir_sl, dir_sl));
	if (direct_dist < 1e-4f)
		direct_dist = 1e-4f;

	out_res->active_portal_idx = -1;
	out_res->direct_distance = direct_dist;
	out_res->path_distance = direct_dist;
	out_res->diffraction_angle_rad = 0.0f;
	out_res->transmission[0] = 1.0f;
	out_res->transmission[1] = 1.0f;
	out_res->transmission[2] = 1.0f;
	out_res->arrival_dir[0] = dir_sl.x / direct_dist;
	out_res->arrival_dir[1] = dir_sl.y / direct_dist;
	out_res->arrival_dir[2] = dir_sl.z / direct_dist;
	out_res->coupling_energy = 0.0f;

	if (!aps || !aps->enabled || aps->num_portals == 0)
		return;

	/* If source and listener are in the same room, direct line of sight applies */
	if (src_room == lis_room) {
		out_res->coupling_energy = 1.0f;
		return;
	}

	float best_energy = -1.0f;
	int32_t best_idx = -1;
	float best_trans[3] = { 0.0f, 0.0f, 0.0f };
	float best_arr[3] = { 0.0f, 0.0f, 1.0f };
	float best_path_dist = direct_dist;
	float best_angle = 0.0f;

	for (uint32_t i = 0; i < aps->num_portals; i++) {
		const struct dsp_acoustic_portal *p = &aps->portals[i];
		if (!p->enabled)
			continue;

		/* Check if portal connects src_room and lis_room */
		bool connects = (p->room_id_front == src_room && p->room_id_back == lis_room) ||
				(p->room_id_front == lis_room && p->room_id_back == src_room);
		if (!connects)
			continue;

		struct dsp_vec3 p_center = { p->center[0], p->center[1], p->center[2] };
		struct dsp_vec3 v_sp = vec3_sub(p_center, source);
		struct dsp_vec3 v_pl = vec3_sub(listener, p_center);

		float d_sp = sqrtf(vec3_dot(v_sp, v_sp));
		float d_pl = sqrtf(vec3_dot(v_pl, v_pl));
		if (d_sp < 1e-4f) d_sp = 1e-4f;
		if (d_pl < 1e-4f) d_pl = 1e-4f;

		struct dsp_vec3 u_sp = { v_sp.x / d_sp, v_sp.y / d_sp, v_sp.z / d_sp };
		struct dsp_vec3 u_pl = { v_pl.x / d_pl, v_pl.y / d_pl, v_pl.z / d_pl };

		/* Diffraction aperture angle: dot between incident ray and outgoing ray */
		float cos_theta = vec3_dot(u_sp, u_pl);
		if (cos_theta > 1.0f) cos_theta = 1.0f;
		if (cos_theta < -1.0f) cos_theta = -1.0f;
		float theta = acosf(cos_theta);

		/* Normal obliquity projections */
		struct dsp_vec3 norm = { p->normal[0], p->normal[1], p->normal[2] };
		float norm_len = sqrtf(vec3_dot(norm, norm));
		if (norm_len > 1e-4f) {
			norm.x /= norm_len;
			norm.y /= norm_len;
			norm.z /= norm_len;
		} else {
			norm.z = 1.0f;
		}

		float cos_psi_in = fabsf(vec3_dot(u_sp, norm));
		float cos_psi_out = fabsf(vec3_dot(u_pl, norm));
		if (cos_psi_in < 0.10f) cos_psi_in = 0.10f;
		if (cos_psi_out < 0.10f) cos_psi_out = 0.10f;

		/* Aperture bending factor: (1 + cos(theta))/2 in [0, 1] */
		float b = 0.5f * (1.0f + cos_theta);
		if (b < 0.0f) b = 0.0f;
		if (b > 1.0f) b = 1.0f;

		float alpha = p->openness;
		if (alpha < 0.0f) alpha = 0.0f;
		if (alpha > 1.0f) alpha = 1.0f;

		/* Frequency-dependent diffraction: gamma_low = 1, gamma_mid = 2, gamma_high = 4 */
		float geom_scale = alpha * cos_psi_in * cos_psi_out;
		float b2 = b * b;
		float b4 = b2 * b2;

		float t0 = geom_scale * b;
		float t1 = geom_scale * b2;
		float t2 = geom_scale * b4;

		if (t0 > 1.0f) t0 = 1.0f;
		if (t1 > 1.0f) t1 = 1.0f;
		if (t2 > 1.0f) t2 = 1.0f;

		float path_dist = d_sp + d_pl;
		float energy = (t0 + t1 + t2) / (3.0f * (path_dist + 0.5f));

		if (energy > best_energy) {
			best_energy = energy;
			best_idx = (int32_t)i;
			best_trans[0] = t0;
			best_trans[1] = t1;
			best_trans[2] = t2;
			best_path_dist = path_dist;
			best_angle = theta;

			/* Sound arrives at listener from portal aperture: p_center - listener */
			struct dsp_vec3 arr = vec3_sub(p_center, listener);
			float arr_len = sqrtf(vec3_dot(arr, arr));
			if (arr_len > 1e-4f) {
				best_arr[0] = arr.x / arr_len;
				best_arr[1] = arr.y / arr_len;
				best_arr[2] = arr.z / arr_len;
			} else {
				best_arr[0] = 0.0f;
				best_arr[1] = 0.0f;
				best_arr[2] = 1.0f;
			}
		}
	}

	if (best_idx >= 0) {
		out_res->active_portal_idx = best_idx;
		out_res->path_distance = best_path_dist;
		out_res->diffraction_angle_rad = best_angle;
		out_res->transmission[0] = best_trans[0];
		out_res->transmission[1] = best_trans[1];
		out_res->transmission[2] = best_trans[2];
		out_res->arrival_dir[0] = best_arr[0];
		out_res->arrival_dir[1] = best_arr[1];
		out_res->arrival_dir[2] = best_arr[2];
		out_res->coupling_energy = best_energy;
	} else {
		/* Rooms are disconnected / no open portal: transmission = 0 */
		out_res->active_portal_idx = -1;
		out_res->transmission[0] = 0.0f;
		out_res->transmission[1] = 0.0f;
		out_res->transmission[2] = 0.0f;
		out_res->coupling_energy = 0.0f;
	}
}

void steamaudio_dsp_acoustic_portals_process(struct dsp_acoustic_portals_state *aps,
					    const float *in,
					    float *out,
					    uint32_t frames,
					    bool muted)
{
	if (!in || !out)
		return;

	if (frames > 256)
		frames = 256;

	/* Bit-exact Step 27 Mute bypass: exact pass-through (out == in) */
	if (muted || !aps || !aps->enabled) {
		memcpy(out, in, frames * sizeof(float));
		return;
	}

	float fs = aps->sample_rate > 0 ? (float)aps->sample_rate : 48000.0f;

	/* 3-band crossover parameters: Low/Mid split at 800Hz, Mid/High split at 4000Hz */
	float w1 = 2.0f * 3.14159265f * 800.0f / fs;
	float a1 = w1 / (1.0f + w1);

	float w2 = 2.0f * 3.14159265f * 4000.0f / fs;
	float a2 = 1.0f / (1.0f + w2);

	/* Determine active portal transmission or default pass-through */
	float g0 = 1.0f;
	float g1 = 1.0f;
	float g2 = 1.0f;

	if (aps->num_portals > 0) {
		/* Default to portal 0 openness scaling if not dynamically queried */
		float alpha = aps->portals[0].openness;
		g0 = alpha;
		g1 = alpha;
		g2 = alpha;
	}

	for (uint32_t i = 0; i < frames; i++) {
		float x = in[i];

		/* 1st-order low-pass filter state */
		aps->filter_states[0] += a1 * (x - aps->filter_states[0]);
		float x_low = aps->filter_states[0];

		/* 1st-order high-pass filter state */
		aps->filter_states[1] = a2 * (aps->filter_states[1] + x - aps->prev_input);
		aps->prev_input = x;
		float x_high = aps->filter_states[1];

		/* Mid-band is the complementary remainder for exact unity sum */
		float x_mid = x - x_low - x_high;

		/* Recombine with 3-band transmission coefficients */
		out[i] = g0 * x_low + g1 * x_mid + g2 * x_high;
	}
}

/* Volumetric Sound Sources & Spatial Soundfield Spread Implementation */
void steamaudio_dsp_volumetric_source_init(struct dsp_volumetric_source_state *vss)
{
	if (!vss)
		return;

	memset(vss, 0, sizeof(*vss));
	vss->sample_rate = 48000;
	vss->speaker_layout = STEAMAUDIO_SPEAKER_LAYOUT_STEREO;
	vss->num_speakers = 2;
	vss->listener_pos[0] = 0.0f;
	vss->listener_pos[1] = 0.0f;
	vss->listener_pos[2] = 0.0f;
	vss->listener_ahead[0] = 0.0f;
	vss->listener_ahead[1] = 0.0f;
	vss->listener_ahead[2] = -1.0f;
	vss->listener_up[0] = 0.0f;
	vss->listener_up[1] = 1.0f;
	vss->listener_up[2] = 0.0f;
	vss->enabled = true;
	vss->flags = 1;
}

void steamaudio_dsp_volumetric_source_set_config(struct dsp_volumetric_source_state *vss,
						const struct sof_steamaudio_volumetric_source_config *cfg)
{
	if (!vss || !cfg)
		return;

	vss->num_sources = cfg->num_sources > STEAMAUDIO_MAX_VOLUMETRIC_SOURCES ?
				STEAMAUDIO_MAX_VOLUMETRIC_SOURCES : cfg->num_sources;

	for (uint32_t i = 0; i < vss->num_sources; i++) {
		vss->sources[i].shape_type = cfg->sources[i].shape_type;
		vss->sources[i].center[0] = cfg->sources[i].center[0];
		vss->sources[i].center[1] = cfg->sources[i].center[1];
		vss->sources[i].center[2] = cfg->sources[i].center[2];
		for (int p = 0; p < 4; p++)
			vss->sources[i].params[p] = cfg->sources[i].params[p];
		vss->sources[i].energy_distribution = cfg->sources[i].energy_distribution;
		vss->sources[i].enabled = cfg->sources[i].enabled;
	}

	vss->listener_pos[0] = cfg->listener_pos[0];
	vss->listener_pos[1] = cfg->listener_pos[1];
	vss->listener_pos[2] = cfg->listener_pos[2];

	vss->listener_ahead[0] = cfg->listener_ahead[0];
	vss->listener_ahead[1] = cfg->listener_ahead[1];
	vss->listener_ahead[2] = cfg->listener_ahead[2];

	vss->listener_up[0] = cfg->listener_up[0];
	vss->listener_up[1] = cfg->listener_up[1];
	vss->listener_up[2] = cfg->listener_up[2];

	vss->speaker_layout = cfg->speaker_layout;
	switch (vss->speaker_layout) {
	case STEAMAUDIO_SPEAKER_LAYOUT_QUAD:
		vss->num_speakers = 4;
		break;
	case STEAMAUDIO_SPEAKER_LAYOUT_5_1:
		vss->num_speakers = 6;
		break;
	case STEAMAUDIO_SPEAKER_LAYOUT_7_1:
		vss->num_speakers = 8;
		break;
	case STEAMAUDIO_SPEAKER_LAYOUT_STEREO:
	default:
		vss->num_speakers = 2;
		break;
	}

	vss->sample_rate = cfg->sample_rate > 0 ? cfg->sample_rate : 48000;
	vss->enabled = (cfg->flags & 1) != 0;
	vss->flags = cfg->flags;
}

void steamaudio_dsp_volumetric_source_evaluate(struct dsp_volumetric_source_state *vss,
					       uint32_t source_idx,
					       struct dsp_volumetric_spread_result *out_res)
{
	if (!vss || !out_res)
		return;

	if (source_idx >= vss->num_sources) {
		memset(out_res, 0, sizeof(*out_res));
		out_res->direct_gain = 1.0f;
		return;
	}

	const struct dsp_volumetric_source *src = &vss->sources[source_idx];
	struct dsp_vec3 lis = { vss->listener_pos[0], vss->listener_pos[1], vss->listener_pos[2] };
	struct dsp_vec3 center = { src->center[0], src->center[1], src->center[2] };
	struct dsp_vec3 closest = center;
	struct dsp_vec3 apparent_centroid = center;

	float dist_to_center = sqrtf(vec3_dot(vec3_sub(lis, center), vec3_sub(lis, center)));
	float direct_dist = dist_to_center;
	float R_eq = 0.0f;
	float spread_angle = 0.0f;
	float spread_factor = 0.0f;

	switch (src->shape_type) {
	case STEAMAUDIO_VOLUMETRIC_SHAPE_POINT: {
		closest = center;
		apparent_centroid = center;
		direct_dist = dist_to_center;
		R_eq = 0.0f;
		spread_angle = 0.0f;
		spread_factor = 0.0f;
		break;
	}
	case STEAMAUDIO_VOLUMETRIC_SHAPE_SPHERE: {
		float R = src->params[0];
		if (R < 0.0f) R = 0.0f;
		R_eq = R;
		if (dist_to_center > 1e-5f) {
			struct dsp_vec3 dir_c_l = vec3_scale(vec3_sub(lis, center), 1.0f / dist_to_center);
			if (dist_to_center >= R) {
				closest = vec3_add(center, vec3_scale(dir_c_l, R));
				direct_dist = dist_to_center - R;
				apparent_centroid = center;
				spread_angle = 2.0f * atan2f(R, dist_to_center);
			} else {
				closest = lis;
				direct_dist = 0.0f;
				apparent_centroid = center;
				spread_angle = 3.14159265f;
			}
		} else {
			closest = lis;
			direct_dist = 0.0f;
			apparent_centroid = center;
			spread_angle = 3.14159265f;
		}
		spread_factor = fminf(1.0f, fmaxf(0.0f, spread_angle / 3.14159265f));
		break;
	}
	case STEAMAUDIO_VOLUMETRIC_SHAPE_BOX: {
		float hx = src->params[0] > 0.0f ? src->params[0] : 0.0f;
		float hy = src->params[1] > 0.0f ? src->params[1] : 0.0f;
		float hz = src->params[2] > 0.0f ? src->params[2] : 0.0f;
		R_eq = (hx + hy + hz) / 3.0f;

		closest.x = fminf(fmaxf(lis.x, center.x - hx), center.x + hx);
		closest.y = fminf(fmaxf(lis.y, center.y - hy), center.y + hy);
		closest.z = fminf(fmaxf(lis.z, center.z - hz), center.z + hz);

		struct dsp_vec3 diff = vec3_sub(lis, closest);
		direct_dist = sqrtf(vec3_dot(diff, diff));
		apparent_centroid = center;

		if (direct_dist < 1e-4f) {
			spread_angle = 3.14159265f;
			spread_factor = 1.0f;
		} else {
			spread_angle = 2.0f * atan2f(R_eq, fmaxf(direct_dist, 0.001f));
			spread_factor = fminf(1.0f, fmaxf(0.0f, spread_angle / 3.14159265f));
		}
		break;
	}
	case STEAMAUDIO_VOLUMETRIC_SHAPE_CAPSULE: {
		struct dsp_vec3 b = { src->params[0], src->params[1], src->params[2] };
		float R = src->params[3] > 0.0f ? src->params[3] : 0.0f;
		R_eq = R;

		struct dsp_vec3 ab = vec3_sub(b, center);
		float ab2 = vec3_dot(ab, ab);
		float t = 0.0f;
		if (ab2 > 1e-6f) {
			t = vec3_dot(vec3_sub(lis, center), ab) / ab2;
			t = fminf(fmaxf(t, 0.0f), 1.0f);
		}
		struct dsp_vec3 seg_pt = vec3_add(center, vec3_scale(ab, t));
		struct dsp_vec3 diff = vec3_sub(lis, seg_pt);
		float d_seg = sqrtf(vec3_dot(diff, diff));
		apparent_centroid = seg_pt;

		if (d_seg <= R) {
			closest = lis;
			direct_dist = 0.0f;
			spread_angle = 3.14159265f;
			spread_factor = 1.0f;
		} else {
			struct dsp_vec3 dir = vec3_scale(diff, 1.0f / d_seg);
			closest = vec3_add(seg_pt, vec3_scale(dir, R));
			direct_dist = d_seg - R;
			spread_angle = 2.0f * atan2f(R, fmaxf(direct_dist, 0.001f));
			spread_factor = fminf(1.0f, fmaxf(0.0f, spread_angle / 3.14159265f));
		}
		break;
	}
	default:
		break;
	}

	/* Effective distance avoiding near-field singularities: sqrt(d^2 + R_eq^2) */
	float d_eff = sqrtf(direct_dist * direct_dist + R_eq * R_eq);
	if (d_eff < 0.01f)
		d_eff = 0.01f;

	out_res->closest_point[0] = closest.x;
	out_res->closest_point[1] = closest.y;
	out_res->closest_point[2] = closest.z;

	out_res->apparent_center[0] = apparent_centroid.x;
	out_res->apparent_center[1] = apparent_centroid.y;
	out_res->apparent_center[2] = apparent_centroid.z;

	out_res->direct_distance = direct_dist;
	out_res->center_distance = dist_to_center;
	out_res->effective_distance = d_eff;
	out_res->spread_angle_rad = spread_angle;
	out_res->spread_factor = spread_factor;

	float S = spread_factor;
	out_res->direct_gain = sqrtf(fmaxf(0.0f, 1.0f - S));
	out_res->diffuse_gain = sqrtf(fmaxf(0.0f, S));

	/* Calculate speaker weights using local listener orientation */
	struct dsp_vec3 ahead = { vss->listener_ahead[0], vss->listener_ahead[1], vss->listener_ahead[2] };
	struct dsp_vec3 up = { vss->listener_up[0], vss->listener_up[1], vss->listener_up[2] };
	ahead = vec3_normalize(ahead);
	up = vec3_normalize(up);
	struct dsp_vec3 right = vec3_cross(ahead, up);
	right = vec3_normalize(right);

	struct dsp_vec3 to_source = vec3_sub(apparent_centroid, lis);
	float to_len = sqrtf(vec3_dot(to_source, to_source));
	struct dsp_vec3 dir_local = { 0.0f, 0.0f, -1.0f };
	if (to_len > 1e-5f) {
		struct dsp_vec3 u = vec3_scale(to_source, 1.0f / to_len);
		dir_local.x = vec3_dot(u, right);
		dir_local.y = vec3_dot(u, up);
		dir_local.z = vec3_dot(u, ahead);
	}

	uint32_t num_spk = vss->num_speakers > 0 ? vss->num_speakers : 2;
	if (num_spk > STEAMAUDIO_MAX_SPEAKERS)
		num_spk = STEAMAUDIO_MAX_SPEAKERS;

	for (int i = 0; i < STEAMAUDIO_MAX_SPEAKERS; i++)
		out_res->speaker_weights[i] = 0.0f;

	if (num_spk == 2) {
		/* Stereo tangent panning law */
		float azimuth_x = fminf(fmaxf(dir_local.x, -1.0f), 1.0f);
		float theta_pan = (azimuth_x + 1.0f) * (3.14159265f * 0.25f); /* [0, pi/2] */
		float w_dir_left = cosf(theta_pan);
		float w_dir_right = sinf(theta_pan);

		/* Diffuse omnidirectional distribution (1/sqrt(N)) */
		float w_diff = 0.70710678f;

		/* Energy-conserving spread blend: W_c = sqrt((1 - S)*W_dir^2 + S*W_diff^2) */
		out_res->speaker_weights[0] = sqrtf((1.0f - S) * (w_dir_left * w_dir_left) + S * (w_diff * w_diff));
		out_res->speaker_weights[1] = sqrtf((1.0f - S) * (w_dir_right * w_dir_right) + S * (w_diff * w_diff));
	} else {
		/* Multichannel: diffuse weight is 1/sqrt(N) for every channel */
		float w_diff_sq = 1.0f / (float)num_spk;
		for (uint32_t c = 0; c < num_spk; c++) {
			float w_dir = (c == 0) ? 0.7071f : (c == 1) ? 0.7071f : 0.0f;
			out_res->speaker_weights[c] = sqrtf((1.0f - S) * (w_dir * w_dir) + S * w_diff_sq);
		}
	}

	vss->results[source_idx] = *out_res;
}

void steamaudio_dsp_volumetric_source_process(struct dsp_volumetric_source_state *vss,
					      const float *in,
					      float *out,
					      uint32_t frames,
					      uint32_t num_channels,
					      bool muted)
{
	if (!in || !out)
		return;

	if (frames > 256)
		frames = 256;

	if (num_channels == 0)
		num_channels = 2;
	if (num_channels > STEAMAUDIO_MAX_SPEAKERS)
		num_channels = STEAMAUDIO_MAX_SPEAKERS;

	/* Bit-exact Step 28 Mute bypass: exact pass-through (out == in) */
	if (muted || !vss || !vss->enabled) {
		for (uint32_t c = 0; c < num_channels; c++)
			memcpy(out + c * frames, in, frames * sizeof(float));
		return;
	}

	float weights[STEAMAUDIO_MAX_SPEAKERS];
	if (vss->num_sources > 0) {
		for (uint32_t c = 0; c < num_channels; c++)
			weights[c] = vss->results[0].speaker_weights[c];
	} else {
		weights[0] = 0.70710678f;
		weights[1] = 0.70710678f;
		for (uint32_t c = 2; c < num_channels; c++)
			weights[c] = 0.0f;
	}

	for (uint32_t c = 0; c < num_channels; c++) {
		float g = weights[c];
		float *dst = out + c * frames;
		for (uint32_t i = 0; i < frames; i++)
			dst[i] = in[i] * g;
	}
}

/* Voice Management & Dynamic Source Prioritization Implementation */

void steamaudio_dsp_source_prioritization_init(struct dsp_source_prioritization_state *sps)
{
	if (!sps)
		return;

	memset(sps, 0, sizeof(*sps));
	sps->max_voices = STEAMAUDIO_MAX_ACTIVE_VOICES;
	sps->min_audible_threshold = 0.001f; /* -60 dB */
	sps->distance_reference = 1.0f;
	sps->distance_max = 100.0f;
	sps->fov_attenuation_bias = 0.5f;
	sps->hysteresis_margin = 0.05f;
	sps->listener_ahead[2] = -1.0f; /* facing negative Z default */
	sps->sample_rate = 48000;
	sps->enabled = true;
	sps->flags = 3; /* Enabled + Filter */

	for (uint32_t i = 0; i < STEAMAUDIO_MAX_PRIORITY_SOURCES; i++) {
		sps->allocations[i].hardware_voice_idx = -1;
		sps->allocations[i].voice_gain = 0.0f;
	}
}

void steamaudio_dsp_source_prioritization_set_config(struct dsp_source_prioritization_state *sps,
						    const struct sof_steamaudio_source_prioritization_config *cfg)
{
	if (!sps || !cfg)
		return;

	uint32_t ns = cfg->num_sources;
	if (ns > STEAMAUDIO_MAX_PRIORITY_SOURCES)
		ns = STEAMAUDIO_MAX_PRIORITY_SOURCES;
	sps->num_sources = ns;

	uint32_t mv = cfg->max_voices;
	if (mv == 0)
		mv = STEAMAUDIO_MAX_ACTIVE_VOICES;
	if (mv > STEAMAUDIO_MAX_ACTIVE_VOICES)
		mv = STEAMAUDIO_MAX_ACTIVE_VOICES;
	sps->max_voices = mv;

	for (uint32_t i = 0; i < ns; i++) {
		sps->sources[i].source_id = cfg->sources[i].source_id;
		sps->sources[i].position[0] = cfg->sources[i].position[0];
		sps->sources[i].position[1] = cfg->sources[i].position[1];
		sps->sources[i].position[2] = cfg->sources[i].position[2];
		sps->sources[i].base_priority = cfg->sources[i].base_priority;
		sps->sources[i].volume = cfg->sources[i].volume;
		sps->sources[i].direct_fraction = cfg->sources[i].direct_fraction;
		sps->sources[i].flags = cfg->sources[i].flags;
		sps->sources[i].enabled = cfg->sources[i].enabled;
	}

	for (int i = 0; i < 3; i++) {
		sps->listener_pos[i] = cfg->listener_pos[i];
		sps->listener_ahead[i] = cfg->listener_ahead[i];
	}

	/* Normalize listener ahead */
	float ahead_len = sqrtf(sps->listener_ahead[0] * sps->listener_ahead[0] +
				sps->listener_ahead[1] * sps->listener_ahead[1] +
				sps->listener_ahead[2] * sps->listener_ahead[2]);
	if (ahead_len > 1e-6f) {
		sps->listener_ahead[0] /= ahead_len;
		sps->listener_ahead[1] /= ahead_len;
		sps->listener_ahead[2] /= ahead_len;
	} else {
		sps->listener_ahead[0] = 0.0f;
		sps->listener_ahead[1] = 0.0f;
		sps->listener_ahead[2] = -1.0f;
	}

	if (cfg->min_audible_threshold > 0.0f)
		sps->min_audible_threshold = cfg->min_audible_threshold;
	if (cfg->distance_reference > 0.0f)
		sps->distance_reference = cfg->distance_reference;
	if (cfg->distance_max > 0.0f)
		sps->distance_max = cfg->distance_max;
	if (cfg->fov_attenuation_bias >= 0.0f)
		sps->fov_attenuation_bias = cfg->fov_attenuation_bias;
	if (cfg->hysteresis_margin >= 0.0f)
		sps->hysteresis_margin = cfg->hysteresis_margin;
	if (cfg->sample_rate > 0)
		sps->sample_rate = cfg->sample_rate;

	sps->enabled = (cfg->flags & 1) != 0;
	sps->flags = cfg->flags;

	steamaudio_dsp_source_prioritization_evaluate(sps);
}

void steamaudio_dsp_source_prioritization_evaluate(struct dsp_source_prioritization_state *sps)
{
	if (!sps)
		return;

	uint32_t ns = sps->num_sources;
	if (ns > STEAMAUDIO_MAX_PRIORITY_SOURCES)
		ns = STEAMAUDIO_MAX_PRIORITY_SOURCES;

	/* 1. Calculate base psychoacoustic priority for each source */
	float effective_scores[STEAMAUDIO_MAX_PRIORITY_SOURCES];

	for (uint32_t i = 0; i < ns; i++) {
		struct dsp_source_priority_input *src = &sps->sources[i];
		struct dsp_source_voice_allocation *alloc = &sps->allocations[i];

		alloc->source_id = src->source_id;
		alloc->was_active = alloc->is_active;

		if (!src->enabled) {
			alloc->calculated_priority = 0.0f;
			alloc->distance = 0.0f;
			alloc->fov_dot = 0.0f;
			effective_scores[i] = 0.0f;
			continue;
		}

		/* Vector from listener to source */
		float dx = src->position[0] - sps->listener_pos[0];
		float dy = src->position[1] - sps->listener_pos[1];
		float dz = src->position[2] - sps->listener_pos[2];
		float dist = sqrtf(dx * dx + dy * dy + dz * dz);
		alloc->distance = dist;

		/* FOV Alignment (dot product with listener ahead) */
		float fov_dot = 0.0f;
		if (dist > 1e-4f) {
			float ux = dx / dist;
			float uy = dy / dist;
			float uz = dz / dist;
			fov_dot = ux * sps->listener_ahead[0] +
				  uy * sps->listener_ahead[1] +
				  uz * sps->listener_ahead[2];
		}
		alloc->fov_dot = fov_dot;

		/* Distance Attenuation */
		float d_ref = sps->distance_reference > 0.0f ? sps->distance_reference : 1.0f;
		float dist_factor = d_ref / fmaxf(dist, d_ref);
		if (sps->distance_max > d_ref && dist > sps->distance_max) {
			float cutoff_factor = fmaxf(0.0f, 1.0f - (dist - sps->distance_max) / sps->distance_max);
			dist_factor *= cutoff_factor;
		}

		/* Direct / Transmission Factor: 20% base + 80% line of sight */
		float direct_factor = 0.2f + 0.8f * fminf(fmaxf(src->direct_fraction, 0.0f), 1.0f);

		/* Psychoacoustic Field of View Factor */
		float fov_factor = 1.0f + sps->fov_attenuation_bias * fmaxf(0.0f, fov_dot);

		/* Focus multiplier for special gameplay targets (Bit 1) */
		float focus_multiplier = (src->flags & 2) ? 1.25f : 1.0f;

		/* Audibility threshold check */
		float audibility = src->volume * dist_factor * direct_factor;
		if (audibility < sps->min_audible_threshold) {
			alloc->calculated_priority = 0.0f;
			effective_scores[i] = 0.0f;
			continue;
		}

		float priority = src->base_priority * src->volume * dist_factor *
				 direct_factor * fov_factor * focus_multiplier;
		if (priority < 0.0f)
			priority = 0.0f;

		alloc->calculated_priority = priority;

		/* Hysteresis bonus: previously active voices get a margin bonus to prevent thrashing */
		float hysteresis_bonus = (alloc->was_active) ? sps->hysteresis_margin : 0.0f;
		effective_scores[i] = priority + hysteresis_bonus;
	}

	/* 2. Top-K Voice Allocation */
	uint32_t max_voices = sps->max_voices;
	if (max_voices > STEAMAUDIO_MAX_ACTIVE_VOICES)
		max_voices = STEAMAUDIO_MAX_ACTIVE_VOICES;

	/* Reset allocation slots */
	for (uint32_t i = 0; i < ns; i++) {
		sps->allocations[i].is_active = 0;
		sps->allocations[i].hardware_voice_idx = -1;
	}

	/* Greedy selection of top-K voices by effective score */
	bool selected[STEAMAUDIO_MAX_PRIORITY_SOURCES];
	memset(selected, 0, sizeof(selected));

	uint32_t voices_allocated = 0;
	while (voices_allocated < max_voices) {
		float best_score = 0.0f;
		int best_idx = -1;

		for (uint32_t i = 0; i < ns; i++) {
			if (!selected[i] && sps->allocations[i].calculated_priority > 0.0f) {
				if (effective_scores[i] > best_score) {
					best_score = effective_scores[i];
					best_idx = (int)i;
				}
			}
		}

		if (best_idx < 0 || best_score <= 0.0f)
			break; /* No more eligible audible sources */

		selected[best_idx] = true;
		sps->allocations[best_idx].is_active = 1;
		sps->allocations[best_idx].hardware_voice_idx = (int32_t)voices_allocated;
		voices_allocated++;
	}

	/* 3. Smooth / Update Voice Gains */
	for (uint32_t i = 0; i < ns; i++) {
		struct dsp_source_voice_allocation *alloc = &sps->allocations[i];
		if (alloc->is_active) {
			/* Voice active: ramp to 1.0f */
			alloc->voice_gain = alloc->was_active ? 1.0f : 0.5f; /* quick smooth fade-in */
			alloc->voice_gain = 1.0f;
		} else {
			/* Culled voice */
			alloc->voice_gain = 0.0f;
		}
	}
}

void steamaudio_dsp_source_prioritization_process(struct dsp_source_prioritization_state *sps,
						 const float *in,
						 float *out,
						 uint32_t frames,
						 uint32_t num_channels,
						 bool muted)
{
	if (!in || !out)
		return;

	if (frames > 256)
		frames = 256;

	if (num_channels == 0)
		num_channels = 2;
	if (num_channels > STEAMAUDIO_MAX_SPEAKERS)
		num_channels = STEAMAUDIO_MAX_SPEAKERS;

	/* Bit-exact Step 29 Mute bypass: exact pass-through (out == in) */
	if (muted || !sps || !sps->enabled) {
		for (uint32_t c = 0; c < num_channels; c++)
			memcpy(out + c * frames, in, frames * sizeof(float));
		return;
	}

	/* Modulate primary channel by top allocated voice's gain */
	float gain = 1.0f;
	if (sps->num_sources > 0) {
		gain = sps->allocations[0].is_active ? sps->allocations[0].voice_gain : 0.0f;
	}

	for (uint32_t c = 0; c < num_channels; c++) {
		float *dst = out + c * frames;
		for (uint32_t i = 0; i < frames; i++)
			dst[i] = in[i] * gain;
	}
}

/* Ground Reflection & Acoustic Multipath Interference Implementation */

void steamaudio_dsp_ground_reflection_init(struct dsp_ground_reflection_state *grs)
{
	if (!grs)
		return;

	memset(grs, 0, sizeof(*grs));
	grs->config.ground_plane_normal[1] = 1.0f; /* Normal is +Y */
	grs->config.sound_speed = 343.0f;
	grs->config.sample_rate = 48000;
	grs->config.material_preset = STEAMAUDIO_GROUND_MATERIAL_CONCRETE;

	/* Default Concrete reflection coefficients: [Low, Mid, High] */
	grs->config.reflection_coeffs[0] = 0.98f;
	grs->config.reflection_coeffs[1] = 0.98f;
	grs->config.reflection_coeffs[2] = 0.97f;

	grs->config.enabled = true;
	grs->config.flags = 3; /* Enabled + Filter */
	grs->sample_rate = 48000;
	grs->enabled = true;
	grs->flags = 3;
}

void steamaudio_dsp_ground_reflection_set_config(struct dsp_ground_reflection_state *grs,
						const struct sof_steamaudio_ground_reflection_config *cfg)
{
	if (!grs || !cfg)
		return;

	for (int i = 0; i < 3; i++) {
		grs->config.ground_plane_pos[i] = cfg->ground_plane_pos[i];
		grs->config.ground_plane_normal[i] = cfg->ground_plane_normal[i];
		grs->config.source_pos[i] = cfg->source_pos[i];
		grs->config.listener_pos[i] = cfg->listener_pos[i];
	}

	/* Normalize ground normal */
	float nlen = sqrtf(grs->config.ground_plane_normal[0] * grs->config.ground_plane_normal[0] +
			   grs->config.ground_plane_normal[1] * grs->config.ground_plane_normal[1] +
			   grs->config.ground_plane_normal[2] * grs->config.ground_plane_normal[2]);
	if (nlen > 1e-6f) {
		grs->config.ground_plane_normal[0] /= nlen;
		grs->config.ground_plane_normal[1] /= nlen;
		grs->config.ground_plane_normal[2] /= nlen;
	} else {
		grs->config.ground_plane_normal[0] = 0.0f;
		grs->config.ground_plane_normal[1] = 1.0f;
		grs->config.ground_plane_normal[2] = 0.0f;
	}

	grs->config.material_preset = cfg->material_preset;

	/* Material preset lookup */
	switch (cfg->material_preset) {
	case STEAMAUDIO_GROUND_MATERIAL_CONCRETE:
		grs->config.reflection_coeffs[0] = 0.98f;
		grs->config.reflection_coeffs[1] = 0.98f;
		grs->config.reflection_coeffs[2] = 0.97f;
		break;
	case STEAMAUDIO_GROUND_MATERIAL_SOIL:
		grs->config.reflection_coeffs[0] = 0.80f;
		grs->config.reflection_coeffs[1] = 0.70f;
		grs->config.reflection_coeffs[2] = 0.55f;
		break;
	case STEAMAUDIO_GROUND_MATERIAL_GRASS:
		grs->config.reflection_coeffs[0] = 0.65f;
		grs->config.reflection_coeffs[1] = 0.40f;
		grs->config.reflection_coeffs[2] = 0.20f;
		break;
	case STEAMAUDIO_GROUND_MATERIAL_WATER:
		grs->config.reflection_coeffs[0] = 0.95f;
		grs->config.reflection_coeffs[1] = 0.96f;
		grs->config.reflection_coeffs[2] = 0.98f;
		break;
	case STEAMAUDIO_GROUND_MATERIAL_WOOD:
		grs->config.reflection_coeffs[0] = 0.85f;
		grs->config.reflection_coeffs[1] = 0.75f;
		grs->config.reflection_coeffs[2] = 0.65f;
		break;
	case STEAMAUDIO_GROUND_MATERIAL_CARPET:
		grs->config.reflection_coeffs[0] = 0.45f;
		grs->config.reflection_coeffs[1] = 0.25f;
		grs->config.reflection_coeffs[2] = 0.10f;
		break;
	default:
		grs->config.reflection_coeffs[0] = cfg->reflection_coeffs[0];
		grs->config.reflection_coeffs[1] = cfg->reflection_coeffs[1];
		grs->config.reflection_coeffs[2] = cfg->reflection_coeffs[2];
		break;
	}

	/* Override with custom coeffs if explicitly provided and not preset */
	if (cfg->reflection_coeffs[0] != 0.0f || cfg->reflection_coeffs[1] != 0.0f) {
		grs->config.reflection_coeffs[0] = cfg->reflection_coeffs[0];
		grs->config.reflection_coeffs[1] = cfg->reflection_coeffs[1];
		grs->config.reflection_coeffs[2] = cfg->reflection_coeffs[2];
	}

	grs->config.sound_speed = cfg->sound_speed > 0.0f ? cfg->sound_speed : 343.0f;
	if (cfg->sample_rate > 0)
		grs->sample_rate = cfg->sample_rate;
	grs->config.sample_rate = grs->sample_rate;

	grs->enabled = (cfg->flags & 1) != 0;
	grs->flags = cfg->flags;
	grs->config.flags = cfg->flags;

	steamaudio_dsp_ground_reflection_evaluate(grs, &grs->result);
}

void steamaudio_dsp_ground_reflection_evaluate(struct dsp_ground_reflection_state *grs,
					       struct dsp_ground_reflection_result *out_res)
{
	if (!grs || !out_res)
		return;

	const struct dsp_ground_reflection_config *cfg = &grs->config;

	/* 1. Heights above ground plane: h = (p - p0) . n */
	float hs = (cfg->source_pos[0] - cfg->ground_plane_pos[0]) * cfg->ground_plane_normal[0] +
		   (cfg->source_pos[1] - cfg->ground_plane_pos[1]) * cfg->ground_plane_normal[1] +
		   (cfg->source_pos[2] - cfg->ground_plane_pos[2]) * cfg->ground_plane_normal[2];

	float hl = (cfg->listener_pos[0] - cfg->ground_plane_pos[0]) * cfg->ground_plane_normal[0] +
		   (cfg->listener_pos[1] - cfg->ground_plane_pos[1]) * cfg->ground_plane_normal[1] +
		   (cfg->listener_pos[2] - cfg->ground_plane_pos[2]) * cfg->ground_plane_normal[2];

	/* 2. Specular Image Source: p_img = p_src - 2 * hs * n */
	out_res->image_source_pos[0] = cfg->source_pos[0] - 2.0f * hs * cfg->ground_plane_normal[0];
	out_res->image_source_pos[1] = cfg->source_pos[1] - 2.0f * hs * cfg->ground_plane_normal[1];
	out_res->image_source_pos[2] = cfg->source_pos[2] - 2.0f * hs * cfg->ground_plane_normal[2];

	/* 3. Direct Distance d1 */
	float dx = cfg->source_pos[0] - cfg->listener_pos[0];
	float dy = cfg->source_pos[1] - cfg->listener_pos[1];
	float dz = cfg->source_pos[2] - cfg->listener_pos[2];
	float d1 = sqrtf(dx * dx + dy * dy + dz * dz);
	out_res->direct_distance = d1;

	/* 4. Reflected Distance d2 */
	float idx = out_res->image_source_pos[0] - cfg->listener_pos[0];
	float idy = out_res->image_source_pos[1] - cfg->listener_pos[1];
	float idz = out_res->image_source_pos[2] - cfg->listener_pos[2];
	float d2 = sqrtf(idx * idx + idy * idy + idz * idz);
	out_res->reflected_distance = d2;

	/* 5. Path length difference & delay */
	float delta_d = d2 - d1;
	if (delta_d < 0.0f)
		delta_d = 0.0f;
	out_res->path_difference = delta_d;

	float c = cfg->sound_speed > 0.0f ? cfg->sound_speed : 343.0f;
	float delta_tau = delta_d / c;
	out_res->delay_seconds = delta_tau;

	uint32_t delay_samples = (uint32_t)(delta_tau * (float)grs->sample_rate + 0.5f);
	if (delay_samples > 255)
		delay_samples = 255;
	out_res->delay_samples = delay_samples;

	/* 6. Grazing angle & specular bounce point */
	float total_h = hs + hl;
	if (total_h > 1e-4f) {
		float t_bounce = hs / total_h;
		out_res->specular_bounce_point[0] = cfg->source_pos[0] + t_bounce * (cfg->listener_pos[0] - cfg->source_pos[0]);
		out_res->specular_bounce_point[1] = cfg->source_pos[1] + t_bounce * (cfg->listener_pos[1] - cfg->source_pos[1]);
		out_res->specular_bounce_point[2] = cfg->source_pos[2] + t_bounce * (cfg->listener_pos[2] - cfg->source_pos[2]);

		/* Project onto plane */
		float p_dist = (out_res->specular_bounce_point[0] - cfg->ground_plane_pos[0]) * cfg->ground_plane_normal[0] +
			       (out_res->specular_bounce_point[1] - cfg->ground_plane_pos[1]) * cfg->ground_plane_normal[1] +
			       (out_res->specular_bounce_point[2] - cfg->ground_plane_pos[2]) * cfg->ground_plane_normal[2];
		out_res->specular_bounce_point[0] -= p_dist * cfg->ground_plane_normal[0];
		out_res->specular_bounce_point[1] -= p_dist * cfg->ground_plane_normal[1];
		out_res->specular_bounce_point[2] -= p_dist * cfg->ground_plane_normal[2];
	}

	float dh_sq = d1 * d1 - (hs - hl) * (hs - hl);
	float dh = dh_sq > 0.0f ? sqrtf(dh_sq) : 0.0f;
	out_res->grazing_angle_rad = atan2f(fabsf(total_h), fmaxf(dh, 1e-4f));

	/* 7. Multi-Band Interference Gains (Low: 400Hz, Mid: 2500Hz, High: 10000Hz) */
	float band_freqs[STEAMAUDIO_NUM_EQ_BANDS] = { 400.0f, 2500.0f, 10000.0f };
	float att = (d2 > 1e-4f) ? (d1 / d2) : 1.0f;
	if (att > 1.0f)
		att = 1.0f;

	float sum_gains = 0.0f;
	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		float R = cfg->reflection_coeffs[b];
		float phi = 2.0f * 3.14159265f * band_freqs[b] * delta_tau;
		float g_raw = sqrtf(fmaxf(0.0f, 1.0f + (R * att) * (R * att) + 2.0f * R * att * cosf(phi)));
		float g_norm = g_raw / (1.0f + fabsf(R) * att);
		out_res->interference_gains[b] = g_norm;
		sum_gains += g_norm;
	}
	out_res->composite_gain = sum_gains / 3.0f;

	grs->result = *out_res;
}

void steamaudio_dsp_ground_reflection_process(struct dsp_ground_reflection_state *grs,
					      const float *in,
					      float *out,
					      uint32_t frames,
					      uint32_t num_channels,
					      bool muted)
{
	if (!in || !out)
		return;

	if (frames > 256)
		frames = 256;

	if (num_channels == 0)
		num_channels = 2;
	if (num_channels > STEAMAUDIO_MAX_SPEAKERS)
		num_channels = STEAMAUDIO_MAX_SPEAKERS;

	/* Bit-exact Step 30 Mute bypass: exact pass-through (out == in) */
	if (muted || !grs || !grs->enabled) {
		for (uint32_t c = 0; c < num_channels; c++)
			memcpy(out + c * frames, in, frames * sizeof(float));
		return;
	}

	uint32_t delay_samples = grs->result.delay_samples;
	float att = (grs->result.reflected_distance > 1e-4f) ?
		    (grs->result.direct_distance / grs->result.reflected_distance) : 1.0f;
	if (att > 1.0f)
		att = 1.0f;

	float r_mid = grs->config.reflection_coeffs[1];
	float ref_gain = r_mid * att;
	float norm = 1.0f / (1.0f + fabsf(ref_gain));

	/* Comb filter delay processing */
	float temp_out[256];
	for (uint32_t i = 0; i < frames; i++) {
		grs->delay_buffer[grs->write_pos] = in[i];

		uint32_t read_pos = (grs->write_pos + 256 - delay_samples) & 255;
		float delayed = grs->delay_buffer[read_pos];

		temp_out[i] = (in[i] + ref_gain * delayed) * norm;

		grs->write_pos = (grs->write_pos + 1) & 255;
	}

	for (uint32_t c = 0; c < num_channels; c++) {
		float *dst = out + c * frames;
		memcpy(dst, temp_out, frames * sizeof(float));
	}
}

/* =========================================================================
 * Phase 43: Spatial Audio True-Peak Limiter & Dynamic Range Control (DRC)
 * ========================================================================= */

void steamaudio_dsp_true_peak_limiter_init(struct dsp_limiter_state *dls)
{
	if (!dls)
		return;

	memset(dls, 0, sizeof(*dls));

	dls->config.threshold_db = -0.5f;
	dls->config.ceiling_db = -0.1f;
	dls->config.knee_width_db = 3.0f;
	dls->config.ratio = 20.0f;
	dls->config.attack_time_ms = 0.5f;
	dls->config.release_time_ms = 50.0f;
	dls->config.makeup_gain_db = 0.0f;
	dls->config.sample_rate = 48000;
	dls->config.enabled = true;
	dls->config.flags = 7; /* Bit 0: enabled, Bit 1: soft knee, Bit 2: true peak */

	dls->envelope_gain = 1.0f;
	dls->prev_peak = 0.0f;
	dls->sample_rate = 48000;
	dls->enabled = true;
	dls->flags = 7;
	dls->makeup_gain_linear = 1.0f;

	float dt_att = 0.5f * 0.001f;
	float dt_rel = 50.0f * 0.001f;
	dls->alpha_attack = expf(-1.0f / (48000.0f * dt_att));
	dls->alpha_release = expf(-1.0f / (48000.0f * dt_rel));

	dls->result.current_gain = 1.0f;
	dls->result.current_gain_db = 0.0f;
	dls->result.max_true_peak = 0.0f;
	dls->result.max_true_peak_db = -100.0f;
	dls->result.gain_reduction_db = 0.0f;
}

void steamaudio_dsp_true_peak_limiter_set_config(struct dsp_limiter_state *dls,
						const struct sof_steamaudio_limiter_config *cfg)
{
	if (!dls || !cfg)
		return;

	dls->config.threshold_db = cfg->threshold_db;
	dls->config.ceiling_db = cfg->ceiling_db;
	dls->config.knee_width_db = cfg->knee_width_db >= 0.0f ? cfg->knee_width_db : 0.0f;
	dls->config.ratio = cfg->ratio >= 1.0f ? cfg->ratio : 20.0f;
	dls->config.attack_time_ms = cfg->attack_time_ms > 0.01f ? cfg->attack_time_ms : 0.5f;
	dls->config.release_time_ms = cfg->release_time_ms > 0.1f ? cfg->release_time_ms : 50.0f;
	dls->config.makeup_gain_db = cfg->makeup_gain_db;
	if (cfg->sample_rate > 0)
		dls->sample_rate = cfg->sample_rate;
	dls->config.sample_rate = dls->sample_rate;

	dls->enabled = (cfg->flags & 1) != 0;
	dls->flags = cfg->flags;
	dls->config.flags = cfg->flags;

	float dt_att = dls->config.attack_time_ms * 0.001f;
	float dt_rel = dls->config.release_time_ms * 0.001f;
	dls->alpha_attack = expf(-1.0f / (dls->sample_rate * dt_att));
	dls->alpha_release = expf(-1.0f / (dls->sample_rate * dt_rel));

	dls->makeup_gain_linear = powf(10.0f, dls->config.makeup_gain_db / 20.0f);
}

void steamaudio_dsp_true_peak_limiter_process(struct dsp_limiter_state *dls,
					      const float *in,
					      float *out,
					      uint32_t frames,
					      uint32_t num_channels,
					      bool muted)
{
	if (!in || !out)
		return;

	if (frames > 256)
		frames = 256;

	if (num_channels == 0)
		num_channels = 2;
	if (num_channels > STEAMAUDIO_MAX_SPEAKERS)
		num_channels = STEAMAUDIO_MAX_SPEAKERS;

	/* Bit-exact Step 31 Mute bypass: exact pass-through (out == in) */
	if (muted || !dls || !dls->enabled) {
		for (uint32_t c = 0; c < num_channels; c++)
			memcpy(out + c * frames, in, frames * sizeof(float));
		return;
	}

	float thresh = dls->config.threshold_db;
	float ceiling = dls->config.ceiling_db;
	float knee = dls->config.knee_width_db;
	float ratio = dls->config.ratio;
	bool soft_knee = (dls->flags & 2) != 0 && (knee > 0.01f);
	bool true_peak = (dls->flags & 4) != 0;

	float max_tp_frame = 0.0f;

	for (uint32_t n = 0; n < frames; n++) {
		/* 1. Detect peak across all output channels at sample n */
		float x_peak = 0.0f;
		for (uint32_t c = 0; c < num_channels; c++) {
			float val = fabsf(in[c * frames + n]);
			if (val > x_peak)
				x_peak = val;
		}

		/* 2. ITU-R BS.1770 True-Peak inter-sample estimate */
		float tp_est = x_peak;
		if (true_peak) {
			float delta = fabsf(x_peak - dls->prev_peak);
			float inter_sample = x_peak + 0.25f * delta;
			if (inter_sample > tp_est)
				tp_est = inter_sample;
		}
		dls->prev_peak = x_peak;

		if (tp_est > max_tp_frame)
			max_tp_frame = tp_est;

		/* 3. Convert to dBFS */
		float level_db = 20.0f * log10f(fmaxf(tp_est, 1e-5f));

		/* 4. Static Soft-Knee Compression / Limiting Characteristic */
		float gr_db = 0.0f;
		if (soft_knee) {
			float lower = thresh - 0.5f * knee;
			float upper = thresh + 0.5f * knee;
			if (level_db <= lower) {
				gr_db = 0.0f;
			} else if (level_db >= upper) {
				gr_db = (1.0f - 1.0f / ratio) * (level_db - thresh);
			} else {
				float delta = level_db - lower;
				gr_db = (1.0f - 1.0f / ratio) * (delta * delta) / (2.0f * knee);
			}
		} else {
			if (level_db > thresh)
				gr_db = (1.0f - 1.0f / ratio) * (level_db - thresh);
			else
				gr_db = 0.0f;
		}

		/* Enforce ceiling */
		if (level_db - gr_db > ceiling)
			gr_db = level_db - ceiling;

		if (gr_db < 0.0f)
			gr_db = 0.0f;

		/* 5. Target Linear Gain */
		float g_target = powf(10.0f, -gr_db / 20.0f);
		if (g_target > 1.0f)
			g_target = 1.0f;
		if (g_target < 1e-4f)
			g_target = 1e-4f;

		/* 6. Decoupled Ballistics (Fast Attack / Smooth Release) */
		if (g_target < dls->envelope_gain)
			dls->envelope_gain = dls->alpha_attack * dls->envelope_gain +
					     (1.0f - dls->alpha_attack) * g_target;
		else
			dls->envelope_gain = dls->alpha_release * dls->envelope_gain +
					     (1.0f - dls->alpha_release) * g_target;

		/* 7. Sidechain Linked Multi-Channel Gain Application */
		float ceiling_linear = powf(10.0f, ceiling / 20.0f);
		float final_gain = dls->envelope_gain * dls->makeup_gain_linear;
		if (x_peak > 1e-5f && final_gain * x_peak > ceiling_linear)
			final_gain = ceiling_linear / x_peak;

		for (uint32_t c = 0; c < num_channels; c++)
			out[c * frames + n] = in[c * frames + n] * final_gain;
	}

	dls->result.current_gain = dls->envelope_gain;
	dls->result.current_gain_db = 20.0f * log10f(fmaxf(dls->envelope_gain, 1e-5f));
	dls->result.max_true_peak = max_tp_frame;
	dls->result.max_true_peak_db = 20.0f * log10f(fmaxf(max_tp_frame, 1e-5f));
	dls->result.gain_reduction_db = -dls->result.current_gain_db;
}

/* =========================================================================
 * Phase 44: Room Modal Resonances & Standing Wave Acoustic Eigenmodes
 * ========================================================================= */

#define SPEED_OF_SOUND_AIR 343.0f

void steamaudio_dsp_room_modes_init(struct dsp_room_modes_state *rms)
{
	if (!rms)
		return;

	memset(rms, 0, sizeof(*rms));

	rms->config.comp_type = STEAMAUDIO_PARAM_ROOM_MODES;
	rms->config.room_dimensions[0] = 7.0f; /* Lx */
	rms->config.room_dimensions[1] = 5.0f; /* Ly */
	rms->config.room_dimensions[2] = 3.0f; /* Lz */
	rms->config.wall_absorption = 0.15f;
	rms->config.source_pos[0] = 0.0f; /* corner position for baseline */
	rms->config.source_pos[1] = 0.0f;
	rms->config.source_pos[2] = 0.0f;
	rms->config.listener_pos[0] = 0.0f;
	rms->config.listener_pos[1] = 0.0f;
	rms->config.listener_pos[2] = 0.0f;
	rms->config.num_modes = 8;
	rms->config.sample_rate = 48000;
	rms->config.flags = 5; /* Bit 0: enabled, Bit 1: axial only (off), Bit 2: auto-tune */

	rms->sample_rate = 48000;
	rms->enabled = true;
	rms->flags = 5;

	steamaudio_dsp_room_modes_set_config(rms, &rms->config);
}

void steamaudio_dsp_room_modes_set_config(struct dsp_room_modes_state *rms,
					 const struct sof_steamaudio_room_modes_config *cfg)
{
	if (!rms || !cfg)
		return;

	rms->config = *cfg;
	if (cfg->sample_rate > 0)
		rms->sample_rate = cfg->sample_rate;
	else
		rms->sample_rate = 48000;

	rms->enabled = (cfg->flags & 1) != 0;
	rms->flags = cfg->flags;

	float lx = cfg->room_dimensions[0] > 0.5f ? cfg->room_dimensions[0] : 7.0f;
	float ly = cfg->room_dimensions[1] > 0.5f ? cfg->room_dimensions[1] : 5.0f;
	float lz = cfg->room_dimensions[2] > 0.5f ? cfg->room_dimensions[2] : 3.0f;
	float alpha = cfg->wall_absorption;
	if (alpha < 0.01f) alpha = 0.01f;
	if (alpha > 0.99f) alpha = 0.99f;

	float vol = lx * ly * lz;
	float surf = 2.0f * (lx * ly + lx * lz + ly * lz);
	float abs_area = surf * alpha;
	float t60 = (0.161f * vol) / (abs_area > 0.01f ? abs_area : 0.01f);
	float f_schroeder = 2000.0f * sqrtf(t60 / (vol > 1.0f ? vol : 1.0f));

	rms->result.room_volume = vol;
	rms->result.t60 = t60;
	rms->result.schroeder_freq = f_schroeder;

	/* Candidate modes pool: axial, tangential, oblique up to order 4 */
	struct room_mode_candidate {
		uint8_t nx, ny, nz;
		float f;
		uint8_t type; /* 0: axial, 1: tangential, 2: oblique */
	} candidates[48];
	uint32_t num_cand = 0;

	/* Axial modes */
	for (int i = 1; i <= 4; i++) {
		if (num_cand < 48) {
			candidates[num_cand].nx = i; candidates[num_cand].ny = 0; candidates[num_cand].nz = 0;
			candidates[num_cand].f = (SPEED_OF_SOUND_AIR * 0.5f) * ((float)i / lx);
			candidates[num_cand].type = 0;
			num_cand++;
		}
		if (num_cand < 48) {
			candidates[num_cand].nx = 0; candidates[num_cand].ny = i; candidates[num_cand].nz = 0;
			candidates[num_cand].f = (SPEED_OF_SOUND_AIR * 0.5f) * ((float)i / ly);
			candidates[num_cand].type = 0;
			num_cand++;
		}
		if (num_cand < 48) {
			candidates[num_cand].nx = 0; candidates[num_cand].ny = 0; candidates[num_cand].nz = i;
			candidates[num_cand].f = (SPEED_OF_SOUND_AIR * 0.5f) * ((float)i / lz);
			candidates[num_cand].type = 0;
			num_cand++;
		}
	}

	/* Tangential modes */
	for (int nx = 1; nx <= 2; nx++) {
		for (int ny = 1; ny <= 2; ny++) {
			if (num_cand < 48) {
				float kx = (float)nx / lx;
				float ky = (float)ny / ly;
				candidates[num_cand].nx = nx; candidates[num_cand].ny = ny; candidates[num_cand].nz = 0;
				candidates[num_cand].f = (SPEED_OF_SOUND_AIR * 0.5f) * sqrtf(kx * kx + ky * ky);
				candidates[num_cand].type = 1;
				num_cand++;
			}
			if (num_cand < 48) {
				float kx = (float)nx / lx;
				float kz = (float)ny / lz;
				candidates[num_cand].nx = nx; candidates[num_cand].ny = 0; candidates[num_cand].nz = ny;
				candidates[num_cand].f = (SPEED_OF_SOUND_AIR * 0.5f) * sqrtf(kx * kx + kz * kz);
				candidates[num_cand].type = 1;
				num_cand++;
			}
		}
	}

	/* Oblique modes */
	for (int nx = 1; nx <= 2; nx++) {
		for (int ny = 1; ny <= 2; ny++) {
			for (int nz = 1; nz <= 2; nz++) {
				if (num_cand < 48) {
					float kx = (float)nx / lx;
					float ky = (float)ny / ly;
					float kz = (float)nz / lz;
					candidates[num_cand].nx = nx; candidates[num_cand].ny = ny; candidates[num_cand].nz = nz;
					candidates[num_cand].f = (SPEED_OF_SOUND_AIR * 0.5f) * sqrtf(kx * kx + ky * ky + kz * kz);
					candidates[num_cand].type = 2;
					num_cand++;
				}
			}
		}
	}

	/* Sort candidate modes by frequency */
	for (uint32_t i = 0; i < num_cand - 1; i++) {
		for (uint32_t j = i + 1; j < num_cand; j++) {
			if (candidates[j].f < candidates[i].f) {
				struct room_mode_candidate tmp = candidates[i];
				candidates[i] = candidates[j];
				candidates[j] = tmp;
			}
		}
	}

	uint32_t max_modes = cfg->num_modes;
	if (max_modes == 0) max_modes = 8;
	if (max_modes > STEAMAUDIO_MAX_ROOM_MODES) max_modes = STEAMAUDIO_MAX_ROOM_MODES;

	uint32_t selected = 0;
	float peak_f = 0.0f;
	float peak_db = -100.0f;

	for (uint32_t i = 0; i < num_cand && selected < max_modes; i++) {
		/* If axial only requested */
		if ((cfg->flags & 2) && candidates[i].type != 0)
			continue;

		uint8_t nx = candidates[i].nx;
		uint8_t ny = candidates[i].ny;
		uint8_t nz = candidates[i].nz;
		float f0 = candidates[i].f;

		/* Spatial pressure distribution */
		float pi = 3.14159265f;
		float psi_src = cosf((float)nx * pi * cfg->source_pos[0] / lx) *
				cosf((float)ny * pi * cfg->source_pos[1] / ly) *
				cosf((float)nz * pi * cfg->source_pos[2] / lz);
		float psi_lis = cosf((float)nx * pi * cfg->listener_pos[0] / lx) *
				cosf((float)ny * pi * cfg->listener_pos[1] / ly) *
				cosf((float)nz * pi * cfg->listener_pos[2] / lz);
		float coupling = psi_src * psi_lis;

		/* Q factor based on mode type and surface absorption */
		float base_q = (candidates[i].type == 0) ? 15.0f :
			       ((candidates[i].type == 1) ? 10.0f : 6.0f);
		float q = base_q / sqrtf(alpha);
		if (q < 2.0f) q = 2.0f;
		if (q > 50.0f) q = 50.0f;

		/* Modal peak gain in dB */
		float max_boost_db = (candidates[i].type == 0) ? 12.0f :
				     ((candidates[i].type == 1) ? 6.0f : 3.0f);
		float gain_db = max_boost_db * coupling;

		if (gain_db > peak_db) {
			peak_db = gain_db;
			peak_f = f0;
		}

		rms->modes[selected].freq = f0;
		rms->modes[selected].q_factor = q;
		rms->modes[selected].gain_db = gain_db;
		rms->modes[selected].gain_linear = powf(10.0f, gain_db / 20.0f);
		rms->modes[selected].nx = nx;
		rms->modes[selected].ny = ny;
		rms->modes[selected].nz = nz;
		rms->modes[selected].type = candidates[i].type;

		/* Design peaking biquad filter (Audio EQ Cookbook) */
		float w0 = 2.0f * pi * f0 / (float)rms->sample_rate;
		if (w0 > 3.1f) w0 = 3.1f;
		float alpha_bq = sinf(w0) / (2.0f * q);
		float a_gain = powf(10.0f, gain_db / 40.0f);

		float b0 = 1.0f + alpha_bq * a_gain;
		float b1 = -2.0f * cosf(w0);
		float b2 = 1.0f - alpha_bq * a_gain;
		float a0 = 1.0f + alpha_bq / a_gain;
		float a1 = -2.0f * cosf(w0);
		float a2 = 1.0f - alpha_bq / a_gain;

		rms->filters[selected].b0 = b0 / a0;
		rms->filters[selected].b1 = b1 / a0;
		rms->filters[selected].b2 = b2 / a0;
		rms->filters[selected].a1 = a1 / a0;
		rms->filters[selected].a2 = a2 / a0;

		/* Reset biquad states */
		memset(rms->filters[selected].x1, 0, sizeof(rms->filters[selected].x1));
		memset(rms->filters[selected].x2, 0, sizeof(rms->filters[selected].x2));
		memset(rms->filters[selected].y1, 0, sizeof(rms->filters[selected].y1));
		memset(rms->filters[selected].y2, 0, sizeof(rms->filters[selected].y2));

		selected++;
	}

	rms->num_active_modes = selected;
	rms->result.num_active_modes = selected;
	rms->result.peak_resonance_freq = peak_f;
	rms->result.peak_resonance_db = peak_db;
}

void steamaudio_dsp_room_modes_process(struct dsp_room_modes_state *rms,
				       const float *in,
				       float *out,
				       uint32_t frames,
				       uint32_t num_channels,
				       bool muted)
{
	if (!in || !out)
		return;

	if (frames > 256)
		frames = 256;

	if (num_channels == 0)
		num_channels = 2;
	if (num_channels > STEAMAUDIO_MAX_SPEAKERS)
		num_channels = STEAMAUDIO_MAX_SPEAKERS;

	/* Bit-exact Step 32 Mute bypass: exact pass-through (out == in) */
	if (muted || !rms || !rms->enabled || rms->num_active_modes == 0) {
		for (uint32_t c = 0; c < num_channels; c++)
			memcpy(out + c * frames, in, frames * sizeof(float));
		return;
	}

	/* Initialize output with input */
	for (uint32_t c = 0; c < num_channels; c++)
		memcpy(out + c * frames, in, frames * sizeof(float));

	/* Cascade active modal resonator filters */
	for (uint32_t m = 0; m < rms->num_active_modes; m++) {
		struct dsp_room_mode_biquad *bq = &rms->filters[m];
		float b0 = bq->b0;
		float b1 = bq->b1;
		float b2 = bq->b2;
		float a1 = bq->a1;
		float a2 = bq->a2;

		for (uint32_t c = 0; c < num_channels; c++) {
			float *ch_buf = out + c * frames;
			float x1 = bq->x1[c];
			float x2 = bq->x2[c];
			float y1 = bq->y1[c];
			float y2 = bq->y2[c];

			for (uint32_t n = 0; n < frames; n++) {
				float x0 = ch_buf[n];
				float y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
				x2 = x1;
				x1 = x0;
				y2 = y1;
				y1 = y0;
				ch_buf[n] = y0;
			}

			bq->x1[c] = x1;
			bq->x2[c] = x2;
			bq->y1[c] = y1;
			bq->y2[c] = y2;
		}
	}
}

/* ====================================================================================================================
 * Phase 45: Atmospheric Turbulence, Wind Advection & Microclimate Acoustic Refraction
 * ==================================================================================================================== */

void steamaudio_dsp_atmospheric_turbulence_init(struct dsp_atmospheric_turbulence_state *ats)
{
	if (!ats)
		return;

	memset(ats, 0, sizeof(*ats));
	ats->sample_rate = 48000;
	ats->enabled = true;
	ats->flags = 1;

	ats->config.comp_type = STEAMAUDIO_PARAM_ATMOSPHERIC_TURBULENCE;
	ats->config.wind_velocity[0] = 0.0f;
	ats->config.wind_velocity[1] = 0.0f;
	ats->config.wind_velocity[2] = 0.0f;
	ats->config.source_pos[0] = 0.0f;
	ats->config.source_pos[1] = 0.0f;
	ats->config.source_pos[2] = 0.0f;
	ats->config.listener_pos[0] = 100.0f;
	ats->config.listener_pos[1] = 0.0f;
	ats->config.listener_pos[2] = 0.0f;
	ats->config.turbulence_intensity = 0.2f;
	ats->config.reference_height = 10.0f;
	ats->config.temperature_c = 20.0f;
	ats->config.sample_rate = 48000;
	ats->config.flags = 1;

	steamaudio_dsp_atmospheric_turbulence_set_config(ats, &ats->config);
}

void steamaudio_dsp_atmospheric_turbulence_set_config(struct dsp_atmospheric_turbulence_state *ats,
						      const struct sof_steamaudio_atmospheric_turbulence_config *cfg)
{
	if (!ats || !cfg)
		return;

	ats->config = *cfg;
	if (cfg->sample_rate > 0)
		ats->sample_rate = cfg->sample_rate;

	ats->enabled = (cfg->flags & 1) != 0;
	ats->flags = cfg->flags;

	/* 1. Calculate nominal speed of sound at temperature */
	float temp_c = cfg->temperature_c > -50.0f ? cfg->temperature_c : 20.0f;
	float c0 = 331.3f * sqrtf(1.0f + temp_c / 273.15f);

	/* 2. Source-to-listener ray vector and distance */
	float dx = cfg->listener_pos[0] - cfg->source_pos[0];
	float dy = cfg->listener_pos[1] - cfg->source_pos[1];
	float dz = cfg->listener_pos[2] - cfg->source_pos[2];
	float dist = sqrtf(dx * dx + dy * dy + dz * dz);
	float rx = 1.0f, ry = 0.0f, rz = 0.0f;
	if (dist > 0.001f) {
		rx = dx / dist;
		ry = dy / dist;
		rz = dz / dist;
	} else {
		dist = 0.001f;
	}

	/* 3. Projected wind velocity along acoustic ray */
	float w_ray = cfg->wind_velocity[0] * rx +
		      cfg->wind_velocity[1] * ry +
		      cfg->wind_velocity[2] * rz;

	/* Effective sound speed along ray */
	float c_eff = c0 + w_ray;
	if (c_eff < 200.0f) c_eff = 200.0f;
	if (c_eff > 500.0f) c_eff = 500.0f;

	/* Propagation delay delta relative to c0 */
	float delay_delta_ms = 1000.0f * (dist / c_eff - dist / c0);

	/* 4. Acoustic Refraction & Upwind Shadow Zone */
	float att_low = 0.0f, att_mid = 0.0f, att_high = 0.0f;
	uint32_t is_shadow = 0;

	if (w_ray < -0.5f) {
		/* Upwind condition: sound rays refract upward -> acoustic shadow zone */
		is_shadow = 1;
		float abs_w = -w_ray;
		float d_scale = dist / 100.0f;
		if (d_scale > 4.0f) d_scale = 4.0f;

		att_low = -0.3f * abs_w * d_scale;
		if (att_low < -6.0f) att_low = -6.0f;

		att_mid = -0.9f * abs_w * d_scale;
		if (att_mid < -18.0f) att_mid = -18.0f;

		att_high = -1.8f * abs_w * d_scale;
		if (att_high < -30.0f) att_high = -30.0f;
	} else if (w_ray > 0.5f) {
		/* Downwind condition: sound rays refract downward -> ducting & ground reinforcement */
		float d_scale = dist / 100.0f;
		if (d_scale > 4.0f) d_scale = 4.0f;

		att_low = 0.05f * w_ray * d_scale;
		if (att_low > 1.5f) att_low = 1.5f;

		att_mid = 0.12f * w_ray * d_scale;
		if (att_mid > 2.5f) att_mid = 2.5f;

		att_high = 0.08f * w_ray * d_scale;
		if (att_high > 1.8f) att_high = 1.8f;
	}

	/* 5. Atmospheric Turbulence & Scintillation Index */
	float turb_int = cfg->turbulence_intensity;
	if (turb_int < 0.0f) turb_int = 0.0f;
	if (turb_int > 1.0f) turb_int = 1.0f;

	float sigma_chi = turb_int * 0.12f * sqrtf(dist / 50.0f);
	if (sigma_chi > 0.35f) sigma_chi = 0.35f;

	ats->result.effective_sound_speed = c_eff;
	ats->result.delay_delta_ms = delay_delta_ms;
	ats->result.shadow_attenuation_db[0] = att_low;
	ats->result.shadow_attenuation_db[1] = att_mid;
	ats->result.shadow_attenuation_db[2] = att_high;
	ats->result.scintillation_index = sigma_chi;
	ats->result.is_upwind_shadow = is_shadow;

	/* 6. Calculate 3-band EQ filter biquad coefficients */
	/* Band 0: Low Shelf at 400 Hz */
	float pi = 3.14159265f;
	float sr = (float)ats->sample_rate;
	float w0_low = 2.0f * pi * 400.0f / sr;
	float a_gain_low = powf(10.0f, att_low / 40.0f);
	float alpha_low = sinf(w0_low) / (2.0f * 0.707f);
	float cos_low = cosf(w0_low);
	float sqrt_a_low = 2.0f * sqrtf(a_gain_low) * alpha_low;

	float b0_l = a_gain_low * ((a_gain_low + 1.0f) - (a_gain_low - 1.0f) * cos_low + sqrt_a_low);
	float b1_l = 2.0f * a_gain_low * ((a_gain_low - 1.0f) - (a_gain_low + 1.0f) * cos_low);
	float b2_l = a_gain_low * ((a_gain_low + 1.0f) - (a_gain_low - 1.0f) * cos_low - sqrt_a_low);
	float a0_l = (a_gain_low + 1.0f) + (a_gain_low - 1.0f) * cos_low + sqrt_a_low;
	float a1_l = -2.0f * ((a_gain_low - 1.0f) + (a_gain_low + 1.0f) * cos_low);
	float a2_l = (a_gain_low + 1.0f) + (a_gain_low - 1.0f) * cos_low - sqrt_a_low;

	ats->eq_filter[0].b0 = b0_l / a0_l;
	ats->eq_filter[0].b1 = b1_l / a0_l;
	ats->eq_filter[0].b2 = b2_l / a0_l;
	ats->eq_filter[0].a1 = a1_l / a0_l;
	ats->eq_filter[0].a2 = a2_l / a0_l;

	/* Band 1: Mid Peaking Filter at 1500 Hz */
	float w0_mid = 2.0f * pi * 1500.0f / sr;
	float a_gain_mid = powf(10.0f, att_mid / 40.0f);
	float alpha_mid = sinf(w0_mid) / (2.0f * 1.0f);
	float cos_mid = cosf(w0_mid);

	float b0_m = 1.0f + alpha_mid * a_gain_mid;
	float b1_m = -2.0f * cos_mid;
	float b2_m = 1.0f - alpha_mid * a_gain_mid;
	float a0_m = 1.0f + alpha_mid / a_gain_mid;
	float a1_m = -2.0f * cos_mid;
	float a2_m = 1.0f - alpha_mid / a_gain_mid;

	ats->eq_filter[1].b0 = b0_m / a0_m;
	ats->eq_filter[1].b1 = b1_m / a0_m;
	ats->eq_filter[1].b2 = b2_m / a0_m;
	ats->eq_filter[1].a1 = a1_m / a0_m;
	ats->eq_filter[1].a2 = a2_m / a0_m;

	/* Band 2: High Shelf at 3500 Hz */
	float w0_high = 2.0f * pi * 3500.0f / sr;
	float a_gain_high = powf(10.0f, att_high / 40.0f);
	float alpha_high = sinf(w0_high) / (2.0f * 0.707f);
	float cos_high = cosf(w0_high);
	float sqrt_a_high = 2.0f * sqrtf(a_gain_high) * alpha_high;

	float b0_h = a_gain_high * ((a_gain_high + 1.0f) + (a_gain_high - 1.0f) * cos_high + sqrt_a_high);
	float b1_h = -2.0f * a_gain_high * ((a_gain_high - 1.0f) + (a_gain_high + 1.0f) * cos_high);
	float b2_h = a_gain_high * ((a_gain_high + 1.0f) + (a_gain_high - 1.0f) * cos_high - sqrt_a_high);
	float a0_h = (a_gain_high + 1.0f) - (a_gain_high - 1.0f) * cos_high + sqrt_a_high;
	float a1_h = 2.0f * ((a_gain_high - 1.0f) - (a_gain_high + 1.0f) * cos_high);
	float a2_h = (a_gain_high + 1.0f) - (a_gain_high - 1.0f) * cos_high - sqrt_a_high;

	ats->eq_filter[2].b0 = b0_h / a0_h;
	ats->eq_filter[2].b1 = b1_h / a0_h;
	ats->eq_filter[2].b2 = b2_h / a0_h;
	ats->eq_filter[2].a1 = a1_h / a0_h;
	ats->eq_filter[2].a2 = a2_h / a0_h;

	/* LFO step for 2.0 Hz eddy turnover */
	ats->lfo_step = 2.0f * pi * 2.0f / sr;
}

void steamaudio_dsp_atmospheric_turbulence_process(struct dsp_atmospheric_turbulence_state *ats,
						   const float *in,
						   float *out,
						   uint32_t frames,
						   uint32_t num_channels,
						   bool muted)
{
	if (!in || !out)
		return;

	if (frames > 256)
		frames = 256;

	if (num_channels == 0)
		num_channels = 2;
	if (num_channels > STEAMAUDIO_MAX_SPEAKERS)
		num_channels = STEAMAUDIO_MAX_SPEAKERS;

	/* Bit-exact Step 33 Mute bypass: exact pass-through (out == in) */
	if (muted || !ats || !ats->enabled) {
		for (uint32_t c = 0; c < num_channels; c++)
			memcpy(out + c * frames, in, frames * sizeof(float));
		return;
	}

	/* Initialize output with input */
	for (uint32_t c = 0; c < num_channels; c++)
		memcpy(out + c * frames, in, frames * sizeof(float));

	/* 1. Apply 3-Band Refraction EQ Filter Cascade */
	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		struct dsp_atmospheric_turbulence_biquad *bq = &ats->eq_filter[b];
		float b0 = bq->b0, b1 = bq->b1, b2 = bq->b2;
		float a1 = bq->a1, a2 = bq->a2;

		for (uint32_t c = 0; c < num_channels; c++) {
			float *ch_buf = out + c * frames;
			float x1 = bq->x1[c], x2 = bq->x2[c];
			float y1 = bq->y1[c], y2 = bq->y2[c];

			for (uint32_t n = 0; n < frames; n++) {
				float x0 = ch_buf[n];
				float y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
				x2 = x1;
				x1 = x0;
				y2 = y1;
				y1 = y0;
				ch_buf[n] = y0;
			}

			bq->x1[c] = x1;
			bq->x2[c] = x2;
			bq->y1[c] = y1;
			bq->y2[c] = y2;
		}
	}

	/* 2. Apply Atmospheric Turbulence Scintillation LFO Modulation */
	float sigma_chi = ats->result.scintillation_index;
	if (sigma_chi > 0.001f) {
		float lfo_phase = ats->lfo_phase;
		float lfo_step = ats->lfo_step;
		const float two_pi = 6.2831853f;

		for (uint32_t n = 0; n < frames; n++) {
			/* Two-tone turbulent eddy fluctuation */
			float mod = 1.0f + sigma_chi * (sinf(lfo_phase) + 0.5f * sinf(1.618f * lfo_phase));
			if (mod < 0.1f) mod = 0.1f;

			for (uint32_t c = 0; c < num_channels; c++) {
				out[c * frames + n] *= mod;
			}

			lfo_phase += lfo_step;
			if (lfo_phase >= two_pi)
				lfo_phase -= two_pi;
		}
		ats->lfo_phase = lfo_phase;
	}
}

/* =========================================================================
 * Surface Acoustic Scattering & Rough Boundary Diffuse Dispersion
 * ========================================================================= */

void steamaudio_dsp_surface_scattering_init(struct dsp_surface_scattering_state *sss)
{
	if (!sss)
		return;

	memset(sss, 0, sizeof(*sss));

	sss->config.comp_type = STEAMAUDIO_PARAM_SURFACE_SCATTERING;
	sss->config.roughness_rms = 0.005f; /* 5mm RMS roughness */
	sss->config.correlation_length = 0.05f; /* 50mm correlation length */
	sss->config.incident_angle_rad = 0.0f; /* Normal incidence */
	sss->config.material_absorption[0] = 0.1f;
	sss->config.material_absorption[1] = 0.1f;
	sss->config.material_absorption[2] = 0.1f;
	sss->config.diffuse_fraction = 0.5f;
	sss->config.dispersion_depth = 0.5f;
	sss->config.surface_area = 1.0f;
	sss->config.distance_to_listener = 5.0f;
	sss->config.sample_rate = 48000;
	sss->config.flags = 3; /* Bit 0: enabled, Bit 1: dispersion enabled */

	sss->sample_rate = 48000;
	sss->enabled = true;
	sss->flags = 3;

	steamaudio_dsp_surface_scattering_set_config(sss, &sss->config);
}

void steamaudio_dsp_surface_scattering_set_config(struct dsp_surface_scattering_state *sss,
						  const struct sof_steamaudio_surface_scattering_config *cfg)
{
	if (!sss || !cfg)
		return;

	sss->config = *cfg;
	sss->sample_rate = cfg->sample_rate > 0 ? cfg->sample_rate : 48000;
	sss->enabled = (cfg->flags & 1) != 0;
	sss->flags = cfg->flags;

	/* 1. Acoustic constants & Rayleigh roughness parameters */
	float c = 343.0f; /* Speed of sound in air (m/s) */
	float f_bands[STEAMAUDIO_NUM_EQ_BANDS] = { 400.0f, 2500.0f, 10000.0f };
	float theta_i = cfg->incident_angle_rad;
	if (theta_i < 0.0f) theta_i = 0.0f;
	if (theta_i > 1.55f) theta_i = 1.55f;
	float cos_theta = cosf(theta_i);

	float sigma_h = cfg->roughness_rms;
	if (sigma_h < 0.0f) sigma_h = 0.0f;
	if (sigma_h > 0.5f) sigma_h = 0.5f;

	float diff_frac = cfg->diffuse_fraction;
	if (diff_frac < 0.0f) diff_frac = 0.0f;
	if (diff_frac > 1.0f) diff_frac = 1.0f;

	const float four_pi = 12.5663706f;
	float total_refl = 0.0f;

	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		float f = f_bands[b];
		float Ra = (four_pi * f / c) * sigma_h * cos_theta;
		if (Ra > 10.0f) Ra = 10.0f;

		/* Kirchhoff-Beckmann scattering coefficient s(f) = (1 - exp(-Ra^2)) * diffuse_fraction */
		float s = (1.0f - expf(-Ra * Ra)) * diff_frac;
		if (s < 0.0f) s = 0.0f;
		if (s > 1.0f) s = 1.0f;
		sss->result.scattering_coeff[b] = s;

		/* Material absorption */
		float alpha = cfg->material_absorption[b];
		if (alpha < 0.0f) alpha = 0.0f;
		if (alpha > 0.999f) alpha = 0.999f;

		float R_energy = 1.0f - alpha;
		float E_spec = R_energy * (1.0f - s);
		float E_diff = R_energy * s;

		float g_spec = sqrtf(fmaxf(0.0f, E_spec));
		float g_diff = sqrtf(fmaxf(0.0f, E_diff));

		sss->result.specular_gain[b] = g_spec;
		sss->result.diffuse_gain[b] = g_diff;
		total_refl += (E_spec + E_diff);
	}

	sss->result.dispersion_delay_ms = (2.0f * sigma_h / c) * 1000.0f;
	sss->result.total_reflected_energy = total_refl / 3.0f;

	/* 2. Configure 4-Stage Schroeder Allpass Dispersion Network */
	uint32_t prime_delays[STEAMAUDIO_SCATTERING_ALLPASS_STAGES] = { 17, 29, 43, 67 };
	float disp_depth = cfg->dispersion_depth;
	if (disp_depth < 0.0f) disp_depth = 0.0f;
	if (disp_depth > 1.0f) disp_depth = 1.0f;
	float ap_gain = 0.55f * disp_depth;

	for (uint32_t ch = 0; ch < STEAMAUDIO_MAX_SPEAKERS; ch++) {
		for (uint32_t st = 0; st < STEAMAUDIO_SCATTERING_ALLPASS_STAGES; st++) {
			sss->allpass[ch][st].delay = prime_delays[st];
			sss->allpass[ch][st].gain = ap_gain;
			if (sss->allpass[ch][st].delay >= STEAMAUDIO_SCATTERING_MAX_DELAY)
				sss->allpass[ch][st].delay = STEAMAUDIO_SCATTERING_MAX_DELAY - 1;
		}
	}
}

void steamaudio_dsp_surface_scattering_process(struct dsp_surface_scattering_state *sss,
					       const float *in,
					       float *out,
					       uint32_t frames,
					       uint32_t num_channels,
					       bool muted)
{
	if (!in || !out)
		return;

	if (frames > 256)
		frames = 256;

	if (num_channels == 0)
		num_channels = 2;
	if (num_channels > STEAMAUDIO_MAX_SPEAKERS)
		num_channels = STEAMAUDIO_MAX_SPEAKERS;

	/* Bit-exact Step 34 Mute bypass: exact pass-through (out == in) */
	if (muted || !sss || !sss->enabled) {
		for (uint32_t c = 0; c < num_channels; c++)
			memcpy(out + c * frames, in, frames * sizeof(float));
		return;
	}

	float g_spec_avg = (sss->result.specular_gain[0] + sss->result.specular_gain[1] + sss->result.specular_gain[2]) / 3.0f;
	float g_diff_avg = (sss->result.diffuse_gain[0] + sss->result.diffuse_gain[1] + sss->result.diffuse_gain[2]) / 3.0f;
	bool apply_dispersion = (sss->flags & 2) && (sss->config.dispersion_depth > 0.001f);

	for (uint32_t c = 0; c < num_channels; c++) {
		float *ch_out = out + c * frames;

		for (uint32_t n = 0; n < frames; n++) {
			float in_sample = in[n];
			float spec_sample = in_sample * g_spec_avg;
			float diff_sample = in_sample * g_diff_avg;

			if (apply_dispersion) {
				/* Pass diffuse sample through 4-stage Schroeder allpass filter */
				for (uint32_t st = 0; st < STEAMAUDIO_SCATTERING_ALLPASS_STAGES; st++) {
					struct dsp_surface_scattering_allpass *ap = &sss->allpass[c][st];
					uint32_t d = ap->delay;
					float g = ap->gain;
					uint32_t r_idx = (ap->index >= d) ? (ap->index - d) : (ap->index + STEAMAUDIO_SCATTERING_MAX_DELAY - d);

					float buf_out = ap->buffer[r_idx];
					float w = diff_sample + g * buf_out;
					diff_sample = -g * w + buf_out;

					ap->buffer[ap->index] = w;
					ap->index = (ap->index + 1) % STEAMAUDIO_SCATTERING_MAX_DELAY;
				}
			}

			ch_out[n] = spec_sample + diff_sample;
		}
	}
}

/* =========================================================================
 * Sound Barrier Edge Diffraction & Maekawa Shadowing
 * ========================================================================= */

void steamaudio_dsp_sound_barrier_init(struct dsp_sound_barrier_state *sbs)
{
	if (!sbs)
		return;

	memset(sbs, 0, sizeof(*sbs));

	sbs->config.comp_type = STEAMAUDIO_PARAM_SOUND_BARRIER;
	sbs->config.barrier_type = 0; /* Single Edge */
	sbs->config.source_pos[0] = 0.0f;
	sbs->config.source_pos[1] = 1.5f;
	sbs->config.source_pos[2] = -5.0f;

	sbs->config.listener_pos[0] = 0.0f;
	sbs->config.listener_pos[1] = 1.5f;
	sbs->config.listener_pos[2] = 5.0f;

	sbs->config.edge_pt0[0] = -10.0f;
	sbs->config.edge_pt0[1] = 3.0f;
	sbs->config.edge_pt0[2] = 0.0f;

	sbs->config.edge_pt1[0] = 10.0f;
	sbs->config.edge_pt1[1] = 3.0f;
	sbs->config.edge_pt1[2] = 0.0f;

	sbs->config.edge2_pt0[0] = -10.0f;
	sbs->config.edge2_pt0[1] = 3.0f;
	sbs->config.edge2_pt0[2] = 1.0f;

	sbs->config.edge2_pt1[0] = 10.0f;
	sbs->config.edge2_pt1[1] = 3.0f;
	sbs->config.edge2_pt1[2] = 1.0f;

	sbs->config.barrier_height = 3.0f;
	sbs->config.barrier_transmission[0] = 0.01f;
	sbs->config.barrier_transmission[1] = 0.01f;
	sbs->config.barrier_transmission[2] = 0.01f;
	sbs->config.flanking_limit_db = 25.0f;
	sbs->config.sample_rate = 48000;
	sbs->config.flags = 1; /* Bit 0: enabled */

	sbs->sample_rate = 48000;
	sbs->enabled = true;
	sbs->flags = 1;

	steamaudio_dsp_sound_barrier_set_config(sbs, &sbs->config);
}

void steamaudio_dsp_sound_barrier_set_config(struct dsp_sound_barrier_state *sbs,
					     const struct sof_steamaudio_sound_barrier_config *cfg)
{
	if (!sbs || !cfg)
		return;

	sbs->config = *cfg;
	sbs->sample_rate = cfg->sample_rate > 0 ? cfg->sample_rate : 48000;
	sbs->enabled = (cfg->flags & 1) != 0;
	sbs->flags = cfg->flags;

	/* 1. Geometry & Path Difference (delta) */
	float sx = cfg->source_pos[0], sy = cfg->source_pos[1], sz = cfg->source_pos[2];
	float lx = cfg->listener_pos[0], ly = cfg->listener_pos[1], lz = cfg->listener_pos[2];

	float dx_sl = lx - sx, dy_sl = ly - sy, dz_sl = lz - sz;
	float d_sl = sqrtf(dx_sl * dx_sl + dy_sl * dy_sl + dz_sl * dz_sl);
	if (d_sl < 1e-4f) d_sl = 1e-4f;

	/* Edge 1 midpoint */
	float e1x = 0.5f * (cfg->edge_pt0[0] + cfg->edge_pt1[0]);
	float e1y = 0.5f * (cfg->edge_pt0[1] + cfg->edge_pt1[1]);
	float e1z = 0.5f * (cfg->edge_pt0[2] + cfg->edge_pt1[2]);

	float dx_se1 = e1x - sx, dy_se1 = e1y - sy, dz_se1 = e1z - sz;
	float d_se1 = sqrtf(dx_se1 * dx_se1 + dy_se1 * dy_se1 + dz_se1 * dz_se1);

	float dx_e1l = lx - e1x, dy_e1l = ly - e1y, dz_e1l = lz - e1z;
	float d_e1l = sqrtf(dx_e1l * dx_e1l + dy_e1l * dy_e1l + dz_e1l * dz_e1l);

	float delta = (d_se1 + d_e1l) - d_sl;

	/* Double edge calculation */
	if ((cfg->flags & 2) || cfg->barrier_type == 1) {
		float e2x = 0.5f * (cfg->edge2_pt0[0] + cfg->edge2_pt1[0]);
		float e2y = 0.5f * (cfg->edge2_pt0[1] + cfg->edge2_pt1[1]);
		float e2z = 0.5f * (cfg->edge2_pt0[2] + cfg->edge2_pt1[2]);

		float dx_e1e2 = e2x - e1x, dy_e1e2 = e2y - e1y, dz_e1e2 = e2z - e1z;
		float d_e1e2 = sqrtf(dx_e1e2 * dx_e1e2 + dy_e1e2 * dy_e1e2 + dz_e1e2 * dz_e1e2);

		float dx_e2l = lx - e2x, dy_e2l = ly - e2y, dz_e2l = lz - e2z;
		float d_e2l = sqrtf(dx_e2l * dx_e2l + dy_e2l * dy_e2l + dz_e2l * dz_e2l);

		float delta_double = (d_se1 + d_e1e2 + d_e2l) - d_sl;
		if (delta_double > delta)
			delta = delta_double;
	}

	if (delta < 0.0f) delta = 0.0f;
	sbs->result.path_difference_m = delta;

	/* Determine shadow zone: ray height at edge plane vs edge height */
	float t_edge = 0.5f;
	if (fabsf(lz - sz) > 1e-4f)
		t_edge = (e1z - sz) / (lz - sz);
	t_edge = fmaxf(0.0f, fminf(1.0f, t_edge));
	float ray_y_at_edge = sy + t_edge * (ly - sy);
	bool in_shadow = (ray_y_at_edge < e1y);
	sbs->result.is_in_shadow = in_shadow ? 1 : 0;

	/* 2. Frequency Bands and Maekawa Attenuation */
	float c = 343.0f;
	float f_bands[STEAMAUDIO_NUM_EQ_BANDS] = { 400.0f, 2500.0f, 10000.0f };
	float flank_limit = cfg->flanking_limit_db > 5.0f ? cfg->flanking_limit_db : 25.0f;

	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		float f = f_bands[b];
		float N = 0.0f;
		float att_db = 0.0f;

		if (in_shadow) {
			N = (2.0f * delta * f) / c;
			if (N < 0.0f) N = 0.0f;
			/* Maekawa formula: Delta L = 10 * log10(3 + 20 * N) */
			att_db = 10.0f * log10f(3.0f + 20.0f * N);
			if (att_db > flank_limit)
				att_db = flank_limit;
		} else {
			/* Illuminated zone: small negative N */
			N = -(2.0f * delta * f) / c;
			if (N < -1.0f) N = -1.0f;
			att_db = 10.0f * log10f(3.0f) * expf(3.0f * N);
			if (att_db < 0.0f) att_db = 0.0f;
		}

		sbs->result.fresnel_number[b] = N;
		sbs->result.barrier_attenuation_db[b] = att_db;

		/* Diffraction linear amplitude */
		float g_diff = powf(10.0f, -att_db / 20.0f);

		/* Barrier transmission */
		float tau = cfg->barrier_transmission[b];
		if (tau < 0.0f) tau = 0.0f;
		if (tau > 1.0f) tau = 1.0f;

		float g_comb = sqrtf(g_diff * g_diff + tau * tau);
		if (g_comb > 1.0f) g_comb = 1.0f;
		sbs->result.combined_gain[b] = g_comb;
	}

	/* 3. Synthesize 3-band Shelf/Peaking Filter Biquads */
	float pi = 3.14159265f;
	float sr = (float)sbs->sample_rate;

	/* Band 0: Low Shelf at 400 Hz */
	float att_low = -sbs->result.barrier_attenuation_db[0];
	float w0_l = 2.0f * pi * 400.0f / sr;
	float a_gain_l = powf(10.0f, att_low / 40.0f);
	float alpha_l = sinf(w0_l) / (2.0f * 0.707f);
	float cos_l = cosf(w0_l);
	float sqrt_a_l = 2.0f * sqrtf(a_gain_l) * alpha_l;

	float b0_l = a_gain_l * ((a_gain_l + 1.0f) - (a_gain_l - 1.0f) * cos_l + sqrt_a_l);
	float b1_l = 2.0f * a_gain_l * ((a_gain_l - 1.0f) - (a_gain_l + 1.0f) * cos_l);
	float b2_l = a_gain_l * ((a_gain_l + 1.0f) - (a_gain_l - 1.0f) * cos_l - sqrt_a_l);
	float a0_l = (a_gain_l + 1.0f) + (a_gain_l - 1.0f) * cos_l + sqrt_a_l;
	float a1_l = -2.0f * ((a_gain_l - 1.0f) + (a_gain_l + 1.0f) * cos_l);
	float a2_l = (a_gain_l + 1.0f) + (a_gain_l - 1.0f) * cos_l - sqrt_a_l;

	sbs->filter[0].b0 = b0_l / a0_l;
	sbs->filter[0].b1 = b1_l / a0_l;
	sbs->filter[0].b2 = b2_l / a0_l;
	sbs->filter[0].a1 = a1_l / a0_l;
	sbs->filter[0].a2 = a2_l / a0_l;

	/* Band 1: Mid Peaking at 2000 Hz */
	float att_mid = -sbs->result.barrier_attenuation_db[1];
	float w0_m = 2.0f * pi * 2000.0f / sr;
	float a_gain_m = powf(10.0f, att_mid / 40.0f);
	float alpha_m = sinf(w0_m) / (2.0f * 1.0f);
	float cos_m = cosf(w0_m);

	float b0_m = 1.0f + alpha_m * a_gain_m;
	float b1_m = -2.0f * cos_m;
	float b2_m = 1.0f - alpha_m * a_gain_m;
	float a0_m = 1.0f + alpha_m / a_gain_m;
	float a1_m = -2.0f * cos_m;
	float a2_m = 1.0f - alpha_m / a_gain_m;

	sbs->filter[1].b0 = b0_m / a0_m;
	sbs->filter[1].b1 = b1_m / a0_m;
	sbs->filter[1].b2 = b2_m / a0_m;
	sbs->filter[1].a1 = a1_m / a0_m;
	sbs->filter[1].a2 = a2_m / a0_m;

	/* Band 2: High Shelf at 4000 Hz */
	float att_high = -sbs->result.barrier_attenuation_db[2];
	float w0_h = 2.0f * pi * 4000.0f / sr;
	float a_gain_h = powf(10.0f, att_high / 40.0f);
	float alpha_h = sinf(w0_h) / (2.0f * 0.707f);
	float cos_h = cosf(w0_h);
	float sqrt_a_h = 2.0f * sqrtf(a_gain_h) * alpha_h;

	float b0_h = a_gain_h * ((a_gain_h + 1.0f) + (a_gain_h - 1.0f) * cos_h + sqrt_a_h);
	float b1_h = -2.0f * a_gain_h * ((a_gain_h - 1.0f) + (a_gain_h + 1.0f) * cos_h);
	float b2_h = a_gain_h * ((a_gain_h + 1.0f) + (a_gain_h - 1.0f) * cos_h - sqrt_a_h);
	float a0_h = (a_gain_h + 1.0f) - (a_gain_h - 1.0f) * cos_h + sqrt_a_h;
	float a1_h = 2.0f * ((a_gain_h - 1.0f) - (a_gain_h + 1.0f) * cos_h);
	float a2_h = (a_gain_h + 1.0f) - (a_gain_h - 1.0f) * cos_h - sqrt_a_h;

	sbs->filter[2].b0 = b0_h / a0_h;
	sbs->filter[2].b1 = b1_h / a0_h;
	sbs->filter[2].b2 = b2_h / a0_h;
	sbs->filter[2].a1 = a1_h / a0_h;
	sbs->filter[2].a2 = a2_h / a0_h;
}

void steamaudio_dsp_sound_barrier_process(struct dsp_sound_barrier_state *sbs,
					  const float *in,
					  float *out,
					  uint32_t frames,
					  uint32_t num_channels,
					  bool muted)
{
	if (!in || !out)
		return;

	if (frames > 256)
		frames = 256;

	if (num_channels == 0)
		num_channels = 2;
	if (num_channels > STEAMAUDIO_MAX_SPEAKERS)
		num_channels = STEAMAUDIO_MAX_SPEAKERS;

	/* Bit-exact Step 35 Mute bypass: exact pass-through (out == in) */
	if (muted || !sbs || !sbs->enabled) {
		for (uint32_t c = 0; c < num_channels; c++)
			memcpy(out + c * frames, in, frames * sizeof(float));
		return;
	}

	for (uint32_t c = 0; c < num_channels; c++)
		memcpy(out + c * frames, in, frames * sizeof(float));

	/* Cascade 3-band shelf/peaking filters */
	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		struct dsp_sound_barrier_biquad *bq = &sbs->filter[b];
		float b0 = bq->b0, b1 = bq->b1, b2 = bq->b2;
		float a1 = bq->a1, a2 = bq->a2;

		for (uint32_t c = 0; c < num_channels; c++) {
			float *ch_buf = out + c * frames;
			float x1 = bq->x1[c], x2 = bq->x2[c];
			float y1 = bq->y1[c], y2 = bq->y2[c];

			for (uint32_t n = 0; n < frames; n++) {
				float x0 = ch_buf[n];
				float y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
				x2 = x1;
				x1 = x0;
				y2 = y1;
				y1 = y0;
				ch_buf[n] = y0;
			}

			bq->x1[c] = x1;
			bq->x2[c] = x2;
			bq->y1[c] = y1;
			bq->y2[c] = y2;
		}
	}
}

/* --------------------------------------------------------------------------------------------------------------------
 * Near-Field HRIR Parallax & Proximity Effect Bass Boost Implementation (Phase 48)
 * --------------------------------------------------------------------------------------------------------------------
 */

void steamaudio_dsp_near_field_init(struct dsp_near_field_state *nfs)
{
	if (!nfs)
		return;

	memset(nfs, 0, sizeof(*nfs));

	nfs->config.comp_type = STEAMAUDIO_PARAM_NEAR_FIELD;
	nfs->config.source_pos[0] = 0.0f;
	nfs->config.source_pos[1] = 0.0f;
	nfs->config.source_pos[2] = 1.0f;
	nfs->config.listener_pos[0] = 0.0f;
	nfs->config.listener_pos[1] = 0.0f;
	nfs->config.listener_pos[2] = 0.0f;
	nfs->config.head_radius = 0.0875f;
	nfs->config.reference_distance = 1.0f;
	nfs->config.bass_boost_limit_db = 18.0f;
	nfs->config.sample_rate = 48000;
	nfs->config.flags = 1 | 2 | 4; /* Enabled | Parallax | BassBoost */

	nfs->sample_rate = 48000;
	nfs->enabled = true;
	nfs->flags = nfs->config.flags;

	steamaudio_dsp_near_field_set_config(nfs, &nfs->config);
}

void steamaudio_dsp_near_field_set_config(struct dsp_near_field_state *nfs,
					  const struct sof_steamaudio_near_field_config *cfg)
{
	if (!nfs || !cfg)
		return;

	nfs->config = *cfg;
	nfs->sample_rate = cfg->sample_rate > 0 ? cfg->sample_rate : 48000;
	nfs->enabled = (cfg->flags & 1) != 0;
	nfs->flags = cfg->flags;

	float sx = cfg->source_pos[0], sy = cfg->source_pos[1], sz = cfg->source_pos[2];
	float lx = cfg->listener_pos[0], ly = cfg->listener_pos[1], lz = cfg->listener_pos[2];

	float dx = sx - lx, dy = sy - ly, dz = sz - lz;
	float r = sqrtf(dx * dx + dy * dy + dz * dz);
	if (r < 1e-4f) r = 1e-4f;
	nfs->result.distance_m = r;

	float a = cfg->head_radius > 0.01f ? cfg->head_radius : 0.0875f;
	float r_ref = cfg->reference_distance > 0.1f ? cfg->reference_distance : 1.0f;
	float max_boost = cfg->bass_boost_limit_db > 1.0f ? cfg->bass_boost_limit_db : 18.0f;

	/* Ear positions relative to listener (assuming head oriented along Z, X is interaural axis) */
	float el_x = lx - a, el_y = ly, el_z = lz;
	float er_x = lx + a, er_y = ly, er_z = lz;

	float dx_l = sx - el_x, dy_l = sy - el_y, dz_l = sz - el_z;
	float r_l = sqrtf(dx_l * dx_l + dy_l * dy_l + dz_l * dz_l);
	if (r_l < 1e-4f) r_l = 1e-4f;

	float dx_r = sx - er_x, dy_r = sy - er_y, dz_r = sz - er_z;
	float r_r = sqrtf(dx_r * dx_r + dy_r * dy_r + dz_r * dz_r);
	if (r_r < 1e-4f) r_r = 1e-4f;

	nfs->result.distance_left_m = r_l;
	nfs->result.distance_right_m = r_r;

	/* Near field threshold */
	bool is_nf = (r < r_ref);
	nfs->result.is_near_field = is_nf ? 1 : 0;

	/* Interaural Level Difference (ILD) divergence */
	float ild_db = fabsf(20.0f * log10f(r_r / r_l));
	nfs->result.ild_boost_db = ild_db;

	/* Spherical wave particle velocity proximity bass boost */
	float boost_db = 0.0f;
	if (is_nf && (cfg->flags & 4)) {
		float c = 343.0f;
		float fc = 250.0f; /* 250 Hz transition frequency */
		float kr = (2.0f * 3.14159265f * fc * r) / c;
		if (kr < 0.01f) kr = 0.01f;
		float ratio = 1.0f / kr;
		float factor = sqrtf(1.0f + ratio * ratio);
		boost_db = 20.0f * log10f(factor);
		if (boost_db > max_boost)
			boost_db = max_boost;
		if (boost_db < 0.0f)
			boost_db = 0.0f;
	}

	nfs->result.bass_boost_db = boost_db;
	nfs->result.bass_boost_gain = powf(10.0f, boost_db / 20.0f);

	/* 2nd-order Low Shelf Filter for proximity bass boost */
	float pi = 3.14159265f;
	float sr = (float)nfs->sample_rate;
	float w0 = 2.0f * pi * 250.0f / sr;
	float A = powf(10.0f, boost_db / 40.0f);
	float alpha = sinf(w0) / (2.0f * 0.707f);
	float cos_w = cosf(w0);
	float sqrt_A = 2.0f * sqrtf(A) * alpha;

	float b0 = A * ((A + 1.0f) - (A - 1.0f) * cos_w + sqrt_A);
	float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w);
	float b2 = A * ((A + 1.0f) - (A - 1.0f) * cos_w - sqrt_A);
	float a0 = (A + 1.0f) + (A - 1.0f) * cos_w + sqrt_A;
	float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w);
	float a2 = (A + 1.0f) + (A - 1.0f) * cos_w - sqrt_A;

	nfs->filter.b0 = b0 / a0;
	nfs->filter.b1 = b1 / a0;
	nfs->filter.b2 = b2 / a0;
	nfs->filter.a1 = a1 / a0;
	nfs->filter.a2 = a2 / a0;
}

void steamaudio_dsp_near_field_process(struct dsp_near_field_state *nfs,
				       const float *in,
				       float *out,
				       uint32_t frames,
				       uint32_t num_channels,
				       bool muted)
{
	if (!in || !out)
		return;

	if (frames > 256)
		frames = 256;

	if (num_channels == 0)
		num_channels = 2;
	if (num_channels > STEAMAUDIO_MAX_SPEAKERS)
		num_channels = STEAMAUDIO_MAX_SPEAKERS;

	/* Bit-exact Step 36 Mute bypass: exact pass-through (out == in) */
	if (muted || !nfs || !nfs->enabled) {
		for (uint32_t c = 0; c < num_channels; c++)
			memcpy(out + c * frames, in, frames * sizeof(float));
		return;
	}

	for (uint32_t c = 0; c < num_channels; c++)
		memcpy(out + c * frames, in, frames * sizeof(float));

	/* If not near field and no bass boost, return pure audio */
	if (!nfs->result.is_near_field && !(nfs->flags & 4))
		return;

	/* Apply low-shelf proximity filter */
	struct dsp_near_field_biquad *bq = &nfs->filter;
	float b0 = bq->b0, b1 = bq->b1, b2 = bq->b2;
	float a1 = bq->a1, a2 = bq->a2;

	for (uint32_t c = 0; c < num_channels; c++) {
		float *ch_buf = out + c * frames;
		float x1 = bq->x1[c], x2 = bq->x2[c];
		float y1 = bq->y1[c], y2 = bq->y2[c];

		for (uint32_t n = 0; n < frames; n++) {
			float x0 = ch_buf[n];
			float y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
			x2 = x1;
			x1 = x0;
			y2 = y1;
			y1 = y0;
			ch_buf[n] = y0;
		}

		bq->x1[c] = x1;
		bq->x2[c] = x2;
		bq->y1[c] = y1;
		bq->y2[c] = y2;
	}

	/* If parallax correction is enabled (flags & 2), apply near-field inverse-distance ILD weighting */
	if ((nfs->flags & 2) && num_channels >= 2) {
		float r_nom = nfs->result.distance_m;
		float r_l = nfs->result.distance_left_m;
		float r_r = nfs->result.distance_right_m;

		float gain_l = r_nom / r_l;
		float gain_r = r_nom / r_r;

		/* Normalize so max channel gain does not exceed +12 dB (4.0x) */
		if (gain_l > 4.0f) gain_l = 4.0f;
		if (gain_r > 4.0f) gain_r = 4.0f;

		float *ch0 = out + 0 * frames;
		float *ch1 = out + 1 * frames;
		for (uint32_t n = 0; n < frames; n++) {
			ch0[n] *= gain_l;
			ch1[n] *= gain_r;
		}
	}
}

/* --------------------------------------------------------------------------------------------------------------------
 * Nonlinear Acoustic Propagation & Shock Wave Crest Distortion Implementation (Phase 49)
 * --------------------------------------------------------------------------------------------------------------------
 */

void steamaudio_dsp_nonlinear_wave_init(struct dsp_nonlinear_wave_state *nws)
{
	if (!nws)
		return;

	memset(nws, 0, sizeof(*nws));

	nws->config.comp_type = STEAMAUDIO_PARAM_NONLINEAR_WAVE;
	nws->config.source_spl_db = 135.0f;
	nws->config.distance_m = 10.0f;
	nws->config.nonlinearity_parameter_beta = 1.20f;
	nws->config.shock_threshold_spl_db = 115.0f;
	nws->config.max_shock_dissipation_db = 12.0f;
	nws->config.sample_rate = 48000;
	nws->config.flags = 1 | 2 | 4; /* Enabled | Steepening | Dissipation */

	nws->sample_rate = 48000;
	nws->enabled = true;
	nws->flags = nws->config.flags;

	steamaudio_dsp_nonlinear_wave_set_config(nws, &nws->config);
}

void steamaudio_dsp_nonlinear_wave_set_config(struct dsp_nonlinear_wave_state *nws,
					      const struct sof_steamaudio_nonlinear_wave_config *cfg)
{
	if (!nws || !cfg)
		return;

	nws->config = *cfg;
	nws->sample_rate = cfg->sample_rate > 0 ? cfg->sample_rate : 48000;
	nws->enabled = (cfg->flags & 1) != 0;
	nws->flags = cfg->flags;

	float spl_src = cfg->source_spl_db;
	if (spl_src < 0.0f) spl_src = 0.0f;

	float d = cfg->distance_m;
	if (d < 0.1f) d = 0.1f;

	/* Geometric spherical spreading attenuation */
	float eff_spl = spl_src - 20.0f * log10f(d >= 1.0f ? d : 1.0f);
	if (eff_spl < 0.0f) eff_spl = 0.0f;
	nws->result.effective_spl_db = eff_spl;

	float thresh = cfg->shock_threshold_spl_db > 80.0f ? cfg->shock_threshold_spl_db : 115.0f;
	float beta = cfg->nonlinearity_parameter_beta > 0.5f ? cfg->nonlinearity_parameter_beta : 1.20f;
	float max_dissipation = cfg->max_shock_dissipation_db > 1.0f ? cfg->max_shock_dissipation_db : 12.0f;

	/* Acoustic pressure amplitude p0 */
	float p0 = 2e-5f * powf(10.0f, spl_src / 20.0f);

	/* Characteristic angular frequency omega_0 (1 kHz reference) */
	float omega0 = 6283.1853f; /* 2 * pi * 1000 */
	/* rho0 * c0^3 ~ 1.204 * (343)^3 ~ 4.858e7 */
	const float rho_c3 = 4.858e7f;

	float x_bar = 10000.0f;
	if (p0 > 1.0f) {
		x_bar = rho_c3 / (beta * omega0 * p0);
		if (x_bar < 0.01f) x_bar = 0.01f;
	}
	nws->result.shock_distance_m = x_bar;

	/* Shock distortion index sigma = d / x_bar */
	float sigma = 0.0f;
	if (spl_src >= thresh) {
		sigma = d / x_bar;
	}
	nws->result.distortion_index_sigma = sigma;
	nws->result.has_shock_formed = (sigma >= 1.0f) ? 1 : 0;

	/* Generated Total Harmonic Distortion (THD) percentage */
	float thd = 0.0f;
	if (sigma > 0.0f) {
		thd = 15.0f * sigma;
		if (thd > 35.0f) thd = 35.0f;
	}
	nws->result.thd_percent = thd;

	/* Thermoviscous shock dissipation */
	float diss_db = 0.0f;
	if (sigma > 1.0f && (cfg->flags & 4)) {
		diss_db = 3.0f * (sigma - 1.0f);
		if (diss_db > max_dissipation)
			diss_db = max_dissipation;
	}
	nws->result.shock_dissipation_db = diss_db;

	/* High Shelf Dissipation Filter at 3000 Hz */
	float pi = 3.14159265f;
	float sr = (float)nws->sample_rate;
	float w0_h = 2.0f * pi * 3000.0f / sr;
	float a_gain_h = powf(10.0f, -diss_db / 40.0f);
	float alpha_h = sinf(w0_h) / (2.0f * 0.707f);
	float cos_h = cosf(w0_h);
	float sqrt_a_h = 2.0f * sqrtf(a_gain_h) * alpha_h;

	float b0_h = a_gain_h * ((a_gain_h + 1.0f) + (a_gain_h - 1.0f) * cos_h + sqrt_a_h);
	float b1_h = -2.0f * a_gain_h * ((a_gain_h - 1.0f) + (a_gain_h + 1.0f) * cos_h);
	float b2_h = a_gain_h * ((a_gain_h + 1.0f) + (a_gain_h - 1.0f) * cos_h - sqrt_a_h);
	float a0_h = (a_gain_h + 1.0f) - (a_gain_h - 1.0f) * cos_h + sqrt_a_h;
	float a1_h = 2.0f * ((a_gain_h - 1.0f) - (a_gain_h + 1.0f) * cos_h);
	float a2_h = (a_gain_h + 1.0f) - (a_gain_h - 1.0f) * cos_h - sqrt_a_h;

	nws->filter.b0 = b0_h / a0_h;
	nws->filter.b1 = b1_h / a0_h;
	nws->filter.b2 = b2_h / a0_h;
	nws->filter.a1 = a1_h / a0_h;
	nws->filter.a2 = a2_h / a0_h;
}

void steamaudio_dsp_nonlinear_wave_process(struct dsp_nonlinear_wave_state *nws,
					   const float *in,
					   float *out,
					   uint32_t frames,
					   uint32_t num_channels,
					   bool muted)
{
	if (!in || !out)
		return;

	if (frames > 256)
		frames = 256;

	if (num_channels == 0)
		num_channels = 2;
	if (num_channels > STEAMAUDIO_MAX_SPEAKERS)
		num_channels = STEAMAUDIO_MAX_SPEAKERS;

	/* Bit-exact Step 37 Mute bypass: exact pass-through (out == in) */
	if (muted || !nws || !nws->enabled) {
		for (uint32_t c = 0; c < num_channels; c++)
			memcpy(out + c * frames, in, frames * sizeof(float));
		return;
	}

	for (uint32_t c = 0; c < num_channels; c++)
		memcpy(out + c * frames, in, frames * sizeof(float));

	float sigma = nws->result.distortion_index_sigma;
	if (sigma <= 0.001f)
		return;

	/* 1. Wave crest steepening / soft-shaper distortion */
	if (nws->flags & 2) {
		float kappa = 0.20f * sigma;
		if (kappa > 0.35f) kappa = 0.35f;

		for (uint32_t c = 0; c < num_channels; c++) {
			float *ch_buf = out + c * frames;
			for (uint32_t n = 0; n < frames; n++) {
				float x = ch_buf[n];
				/* Burgers steepening polynomial: x + kappa*x^2 - (kappa^2/3)*x^3 */
				float y = x + kappa * (x * x) - (kappa * kappa * 0.333333f) * (x * x * x);
				ch_buf[n] = y;
			}
		}
	}

	/* 2. Thermoviscous shock dissipation high-cut cascade */
	if ((nws->flags & 4) && sigma > 1.0f) {
		struct dsp_nonlinear_wave_biquad *bq = &nws->filter;
		float b0 = bq->b0, b1 = bq->b1, b2 = bq->b2;
		float a1 = bq->a1, a2 = bq->a2;

		for (uint32_t c = 0; c < num_channels; c++) {
			float *ch_buf = out + c * frames;
			float x1 = bq->x1[c], x2 = bq->x2[c];
			float y1 = bq->y1[c], y2 = bq->y2[c];

			for (uint32_t n = 0; n < frames; n++) {
				float x0 = ch_buf[n];
				float y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
				x2 = x1;
				x1 = x0;
				y2 = y1;
				y1 = y0;
				ch_buf[n] = y0;
			}

			bq->x1[c] = x1;
			bq->x2[c] = x2;
			bq->y1[c] = y1;
			bq->y2[c] = y2;
		}
	}
}

/* Autonomous DSP-Centric Engine API */
void steamaudio_dsp_derive_raw_scene(struct steamaudio_comp_data *cd,
				     const struct raw_scene_packet *scene)
{
	if (!cd || !scene)
		return;

	cd->num_raw_emitters = scene->num_emitters;

	/* 1. Update listener orientation matrix for Ambisonics & Pathing */
	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			cd->ambisonics.rotation[r][c] = scene->listener_rotation[r][c];
			cd->pathing.rotation[r][c] = scene->listener_rotation[r][c];
		}
	}

	/* 2. Configure Autonomous Source Prioritization & Voice Management */
	cd->source_prioritization.listener_pos[0] = scene->listener_pos[0];
	cd->source_prioritization.listener_pos[1] = scene->listener_pos[1];
	cd->source_prioritization.listener_pos[2] = scene->listener_pos[2];

	/* Orientation column/row 2 is ahead vector */
	cd->source_prioritization.listener_ahead[0] = scene->listener_rotation[0][2];
	cd->source_prioritization.listener_ahead[1] = scene->listener_rotation[1][2];
	cd->source_prioritization.listener_ahead[2] = scene->listener_rotation[2][2];

	/* Run autonomous 3-Tier Voice LOD classification for up to 256 emitters */
	uint8_t lod_tiers[STEAMAUDIO_MAX_LOD_SOURCES];
	steamaudio_dsp_voice_lod_classify(&cd->voice_lod, scene, lod_tiers);

	uint32_t count = scene->num_emitters;
	if (count > STEAMAUDIO_MAX_PRIORITY_SOURCES)
		count = STEAMAUDIO_MAX_PRIORITY_SOURCES;
	cd->source_prioritization.num_sources = count;

	for (uint32_t i = 0; i < count; i++) {
		struct dsp_source_priority_input *src = &cd->source_prioritization.sources[i];
		src->source_id = scene->emitters[i].source_id;
		src->position[0] = scene->emitters[i].pos[0];
		src->position[1] = scene->emitters[i].pos[1];
		src->position[2] = scene->emitters[i].pos[2];
		src->base_priority = 1.0f;
		float spl = scene->emitters[i].source_spl_db;
		if (spl < 0.0f)
			spl = 0.0f;
		src->volume = powf(10.0f, (spl - 90.0f) * 0.05f);
		src->direct_fraction = 1.0f;
		src->flags = scene->emitters[i].flags;
		src->enabled = 1;
	}

	steamaudio_dsp_source_prioritization_evaluate(&cd->source_prioritization);

	/* 3. Derive physical acoustic parameters for primary active emitter */
	if (count > 0) {
		const struct raw_emitter_descriptor *prim = &scene->emitters[0];
		float dx = prim->pos[0] - scene->listener_pos[0];
		float dy = prim->pos[1] - scene->listener_pos[1];
		float dz = prim->pos[2] - scene->listener_pos[2];
		float dist = sqrtf(dx * dx + dy * dy + dz * dz);
		float inv_dist = (dist > 1e-6f) ? (1.0f / dist) : 0.0f;
		float wx = dx * inv_dist;
		float wy = dy * inv_dist;
		float wz = dz * inv_dist;

		/* Rotate world direction into listener local coordinates: loc = R^T * w */
		float lx = scene->listener_rotation[0][0] * wx +
			   scene->listener_rotation[1][0] * wy +
			   scene->listener_rotation[2][0] * wz;
		float ly = scene->listener_rotation[0][1] * wx +
			   scene->listener_rotation[1][1] * wy +
			   scene->listener_rotation[2][1] * wz;
		float lz = scene->listener_rotation[0][2] * wx +
			   scene->listener_rotation[1][2] * wy +
			   scene->listener_rotation[2][2] * wz;

		cd->binaural.direction[0] = lx;
		cd->binaural.direction[1] = ly;
		cd->binaural.direction[2] = lz;
		cd->binaural.spatial_blend = 1.0f;

		float pdir[3] = { lx, ly, lz };
		steamaudio_dsp_panning_set_direction(&cd->panning, pdir);

		/* Atmospheric absorption */
		if (scene->ambient_temp_c > 0.0f || scene->ambient_humidity > 0.0f) {
			cd->atmosphere.temperature_c = scene->ambient_temp_c;
			cd->atmosphere.relative_humidity = scene->ambient_humidity;
			cd->atmosphere.pressure_kpa = 101.325f;
			cd->atmosphere.enabled = true;
			cd->atmosphere.flags = 1;
			steamaudio_dsp_calculate_atmosphere(
				cd->atmosphere.temperature_c,
				cd->atmosphere.relative_humidity,
				cd->atmosphere.pressure_kpa,
				&cd->atmosphere.speed_of_sound,
				cd->atmosphere.absorption_coefficients);
		}

		/* Nonlinear wave propagation & Shock distortion */
		cd->nonlinear_wave.config.source_spl_db = prim->source_spl_db;
		cd->nonlinear_wave.config.distance_m = dist;
		cd->nonlinear_wave.config.flags = prim->flags | 1;
		steamaudio_dsp_nonlinear_wave_set_config(&cd->nonlinear_wave, &cd->nonlinear_wave.config);

		/* Near-field physical modeling */
		cd->near_field.config.source_pos[0] = prim->pos[0];
		cd->near_field.config.source_pos[1] = prim->pos[1];
		cd->near_field.config.source_pos[2] = prim->pos[2];
		cd->near_field.config.listener_pos[0] = scene->listener_pos[0];
		cd->near_field.config.listener_pos[1] = scene->listener_pos[1];
		cd->near_field.config.listener_pos[2] = scene->listener_pos[2];
		cd->near_field.config.flags = 1;
		steamaudio_dsp_near_field_set_config(&cd->near_field, &cd->near_field.config);

		/* Directivity */
		if (prim->directivity_weight > 0.0f) {
			cd->directivity.dipole_weight = prim->directivity_weight;
			cd->directivity.dipole_power = 1.0f;
			cd->directivity.source_pos[0] = prim->pos[0];
			cd->directivity.source_pos[1] = prim->pos[1];
			cd->directivity.source_pos[2] = prim->pos[2];
			cd->directivity.listener_pos[0] = scene->listener_pos[0];
			cd->directivity.listener_pos[1] = scene->listener_pos[1];
			cd->directivity.listener_pos[2] = scene->listener_pos[2];
			cd->directivity.calculated_gain = steamaudio_dsp_calculate_directivity(
				cd->directivity.source_pos,
				cd->directivity.source_ahead,
				cd->directivity.listener_pos,
				cd->directivity.dipole_weight,
				cd->directivity.dipole_power);
		}
	}

	/* 4. Room Modal Resonance Derivation */
	if (scene->room_dimensions[0] > 0.5f &&
	    scene->room_dimensions[1] > 0.5f &&
	    scene->room_dimensions[2] > 0.5f) {
		struct sof_steamaudio_room_modes_config rmc;
		memset(&rmc, 0, sizeof(rmc));
		rmc.comp_type = STEAMAUDIO_PARAM_ROOM_MODES;
		rmc.room_dimensions[0] = scene->room_dimensions[0];
		rmc.room_dimensions[1] = scene->room_dimensions[1];
		rmc.room_dimensions[2] = scene->room_dimensions[2];
		rmc.wall_absorption = 0.15f;
		rmc.source_pos[0] = count > 0 ? scene->emitters[0].pos[0] : 1.0f;
		rmc.source_pos[1] = count > 0 ? scene->emitters[0].pos[1] : 1.0f;
		rmc.source_pos[2] = count > 0 ? scene->emitters[0].pos[2] : 1.0f;
		rmc.listener_pos[0] = scene->listener_pos[0];
		rmc.listener_pos[1] = scene->listener_pos[1];
		rmc.listener_pos[2] = scene->listener_pos[2];
		rmc.num_modes = 8;
		rmc.sample_rate = cd->sample_rate ? cd->sample_rate : 48000;
		rmc.flags = 1;
		steamaudio_dsp_room_modes_set_config(&cd->room_modes, &rmc);
	}
}

void steamaudio_dsp_update_cycle_governor(struct steamaudio_comp_data *cd)
{
	if (!cd)
		return;

	uint32_t now = (uint32_t)sof_cycle_get_64();

	if (cd->dsp_cycle_start != 0) {
		uint32_t elapsed = now - cd->dsp_cycle_start;
		cd->dsp_cycle_last_frame = elapsed;
		if (cd->dsp_cycle_moving_avg == 0)
			cd->dsp_cycle_moving_avg = elapsed;
		else
			cd->dsp_cycle_moving_avg = (cd->dsp_cycle_moving_avg * 7 + elapsed) >> 3;

		/* Cycle budget thresholds at 800 MHz (1ms buffer = 800,000 cycles) */
		const uint32_t CYCLE_BUDGET_HIGH = 640000;
		const uint32_t CYCLE_BUDGET_LOW = 320000;

		if (cd->dsp_cycle_moving_avg > CYCLE_BUDGET_HIGH) {
			if (cd->dsp_shedding_level < 2)
				cd->dsp_shedding_level++;
		} else if (cd->dsp_cycle_moving_avg < CYCLE_BUDGET_LOW) {
			if (cd->dsp_shedding_level > 0)
				cd->dsp_shedding_level--;
		}

		/* Adaptive load shedding:
		 * Level 0: Full fidelity (Early reflections 32 taps, hybrid reverb active)
		 * Level 1: Light LOD (Early reflections 16 taps, hybrid reverb active)
		 * Level 2: Heavy LOD (Early reflections bypassed, parametric reverb)
		 */
		if (cd->dsp_shedding_level >= 2) {
			cd->early_reflections.enabled = false;
			cd->hybrid.active = false;
		} else if (cd->dsp_shedding_level == 1) {
			cd->early_reflections.enabled = true;
			if (cd->early_reflections.num_taps > 16)
				cd->early_reflections.num_taps = 16;
			cd->hybrid.active = true;
		} else {
			cd->early_reflections.enabled = true;
			cd->hybrid.active = true;
		}
	}

	cd->dsp_cycle_start = (uint32_t)sof_cycle_get_64();
}

/* Playback-to-Capture Loopback & Battle Bleed Mixer Implementation */
void steamaudio_dsp_battle_bleed_init(struct dsp_battle_bleed_state *bbs, uint32_t sample_rate)
{
	if (!bbs)
		return;

	memset(bbs, 0, sizeof(*bbs));
	bbs->sample_rate = sample_rate ? sample_rate : 48000;
	bbs->config.comp_type = STEAMAUDIO_PARAM_BATTLE_BLEED;
	bbs->config.bleed_volume = 0.15f;
	bbs->config.ducking_depth_db = 12.0f;
	bbs->config.ducking_threshold_db = -30.0f;
	bbs->config.attack_time_ms = 5.0f;
	bbs->config.release_time_ms = 150.0f;
	bbs->config.sample_rate = bbs->sample_rate;
	bbs->config.flags = 3; /* Bit 0: enabled, Bit 1: helmet filter enabled */

	bbs->duck_gain_current = 1.0f;
	bbs->env_mic = 0.0f;
	bbs->result.current_ducking_gain = 1.0f;
	bbs->result.current_ducking_db = 0.0f;
	bbs->result.mic_envelope_db = -96.0f;
	bbs->result.is_speaking = 0;

	steamaudio_dsp_battle_bleed_set_config(bbs, &bbs->config);
}

void steamaudio_dsp_battle_bleed_set_config(struct dsp_battle_bleed_state *bbs,
					    const struct sof_steamaudio_battle_bleed_config *cfg)
{
	if (!bbs || !cfg)
		return;

	bbs->config = *cfg;
	bbs->sample_rate = cfg->sample_rate > 0 ? cfg->sample_rate : 48000;
	bbs->enabled = (cfg->flags & 1) != 0;
	bbs->helmet_filter_enabled = (cfg->flags & 2) != 0;

	float fs = (float)bbs->sample_rate;
	float dt_att = cfg->attack_time_ms > 0.1f ? cfg->attack_time_ms * 0.001f : 0.005f;
	float dt_rel = cfg->release_time_ms > 1.0f ? cfg->release_time_ms * 0.001f : 0.150f;

	bbs->alpha_attack = expf(-1.0f / (fs * dt_att));
	bbs->alpha_release = expf(-1.0f / (fs * dt_rel));

	float duck_depth = cfg->ducking_depth_db >= 0.0f ? cfg->ducking_depth_db : 12.0f;
	bbs->ducking_min_gain = powf(10.0f, -duck_depth / 20.0f);
	bbs->ducking_threshold_lin = powf(10.0f, cfg->ducking_threshold_db / 20.0f);

	/* 1. Calculate 150 Hz 2nd-order Butterworth High-Pass Biquad */
	float f_hp = 150.0f;
	float w0_hp = 2.0f * 3.14159265f * f_hp / fs;
	float cos_hp = cosf(w0_hp);
	float sin_hp = sinf(w0_hp);
	float alpha_hp = sin_hp / (2.0f * 0.70710678f);

	float a0_hp = 1.0f + alpha_hp;
	float inv_a0_hp = 1.0f / a0_hp;
	bbs->hp_filter.b0 = ((1.0f + cos_hp) * 0.5f) * inv_a0_hp;
	bbs->hp_filter.b1 = (-(1.0f + cos_hp)) * inv_a0_hp;
	bbs->hp_filter.b2 = ((1.0f + cos_hp) * 0.5f) * inv_a0_hp;
	bbs->hp_filter.a1 = (-2.0f * cos_hp) * inv_a0_hp;
	bbs->hp_filter.a2 = (1.0f - alpha_hp) * inv_a0_hp;

	/* 2. Calculate 4000 Hz 2nd-order Butterworth Low-Pass Biquad */
	float f_lp = 4000.0f;
	float w0_lp = 2.0f * 3.14159265f * f_lp / fs;
	float cos_lp = cosf(w0_lp);
	float sin_lp = sinf(w0_lp);
	float alpha_lp = sin_lp / (2.0f * 0.70710678f);

	float a0_lp = 1.0f + alpha_lp;
	float inv_a0_lp = 1.0f / a0_lp;
	bbs->lp_filter.b0 = ((1.0f - cos_lp) * 0.5f) * inv_a0_lp;
	bbs->lp_filter.b1 = (1.0f - cos_lp) * inv_a0_lp;
	bbs->lp_filter.b2 = ((1.0f - cos_lp) * 0.5f) * inv_a0_lp;
	bbs->lp_filter.a1 = (-2.0f * cos_lp) * inv_a0_lp;
	bbs->lp_filter.a2 = (1.0f - alpha_lp) * inv_a0_lp;
}

void steamaudio_dsp_battle_bleed_process(struct dsp_battle_bleed_state *bbs,
					 const float *in_playback,
					 const float *in_mic,
					 float *out_capture,
					 uint32_t frames,
					 uint32_t num_channels,
					 bool muted)
{
	if (!out_capture)
		return;

	if (frames > 256)
		frames = 256;
	if (num_channels == 0)
		num_channels = 2;
	if (num_channels > STEAMAUDIO_MAX_SPEAKERS)
		num_channels = STEAMAUDIO_MAX_SPEAKERS;

	/* Bit-exact Mute bypass (Step 38): capture output = mic input */
	if (muted || !bbs || !bbs->enabled) {
		for (uint32_t c = 0; c < num_channels; c++) {
			if (in_mic)
				memcpy(out_capture + c * frames, in_mic + c * frames, frames * sizeof(float));
			else
				memset(out_capture + c * frames, 0, frames * sizeof(float));
		}
		return;
	}

	float bleed_vol = bbs->config.bleed_volume;
	bool use_filter = bbs->helmet_filter_enabled;
	float ga = bbs->alpha_attack;
	float gr = bbs->alpha_release;
	float thresh_lin = bbs->ducking_threshold_lin;
	float min_gain = bbs->ducking_min_gain;

	float env = bbs->env_mic;
	float duck_gain = bbs->duck_gain_current;
	uint32_t speech_count = 0;

	for (uint32_t n = 0; n < frames; n++) {
		/* 1. Detect microphone peak across channels */
		float mic_peak = 0.0f;
		if (in_mic) {
			for (uint32_t c = 0; c < num_channels; c++) {
				float val = fabsf(in_mic[c * frames + n]);
				if (val > mic_peak)
					mic_peak = val;
			}
		}

		/* 2. Sidechain envelope tracking */
		if (mic_peak > env)
			env = mic_peak;
		else
			env = ga * env + (1.0f - ga) * mic_peak;

		/* 3. Dynamic voice ducking calculation */
		float target_duck = 1.0f;
		if (env > thresh_lin) {
			speech_count++;
			float factor = (env - thresh_lin) / (thresh_lin + 1e-6f);
			if (factor > 1.0f) factor = 1.0f;
			target_duck = 1.0f - (1.0f - min_gain) * factor;
		}

		if (target_duck < duck_gain)
			duck_gain = ga * duck_gain + (1.0f - ga) * target_duck;
		else
			duck_gain = gr * duck_gain + (1.0f - gr) * target_duck;

		/* 4. Filter and blend playback audio into capture */
		for (uint32_t c = 0; c < num_channels; c++) {
			float pb = in_playback ? in_playback[c * frames + n] : 0.0f;
			float filtered_pb = pb;

			if (use_filter) {
				/* Highpass 150 Hz */
				float hp_y = bbs->hp_filter.b0 * pb +
					     bbs->hp_filter.b1 * bbs->hp_filter.x1[c] +
					     bbs->hp_filter.b2 * bbs->hp_filter.x2[c] -
					     bbs->hp_filter.a1 * bbs->hp_filter.y1[c] -
					     bbs->hp_filter.a2 * bbs->hp_filter.y2[c];
				bbs->hp_filter.x2[c] = bbs->hp_filter.x1[c];
				bbs->hp_filter.x1[c] = pb;
				bbs->hp_filter.y2[c] = bbs->hp_filter.y1[c];
				bbs->hp_filter.y1[c] = hp_y;

				/* Lowpass 4000 Hz */
				float lp_y = bbs->lp_filter.b0 * hp_y +
					     bbs->lp_filter.b1 * bbs->lp_filter.x1[c] +
					     bbs->lp_filter.b2 * bbs->lp_filter.x2[c] -
					     bbs->lp_filter.a1 * bbs->lp_filter.y1[c] -
					     bbs->lp_filter.a2 * bbs->lp_filter.y2[c];
				bbs->lp_filter.x2[c] = bbs->lp_filter.x1[c];
				bbs->lp_filter.x1[c] = hp_y;
				bbs->lp_filter.y2[c] = bbs->lp_filter.y1[c];
				bbs->lp_filter.y1[c] = lp_y;

				filtered_pb = lp_y;
			}

			float bleed = filtered_pb * bleed_vol * duck_gain;
			float mic = in_mic ? in_mic[c * frames + n] : 0.0f;
			float mixed = mic + bleed;

			/* Soft saturation safeguard */
			if (mixed > 1.0f) mixed = 1.0f;
			else if (mixed < -1.0f) mixed = -1.0f;

			out_capture[c * frames + n] = mixed;
		}
	}

	bbs->env_mic = env;
	bbs->duck_gain_current = duck_gain;
	bbs->result.current_ducking_gain = duck_gain;
	bbs->result.current_ducking_db = 20.0f * log10f(duck_gain > 1e-4f ? duck_gain : 1e-4f);
	bbs->result.mic_envelope_db = 20.0f * log10f(env > 1e-5f ? env : 1e-5f);
	bbs->result.is_speaking = (speech_count > (frames / 4)) ? 1 : 0;
}

/* High Polyphony Voice Scaling & DSP 3-Tier Level-of-Detail (LOD) Implementation */

void steamaudio_dsp_voice_lod_init(struct dsp_voice_lod_state *vls, uint32_t sample_rate)
{
	if (!vls)
		return;

	memset(vls, 0, sizeof(*vls));
	vls->config.comp_type = STEAMAUDIO_PARAM_VOICE_LOD;
	vls->config.enabled = 1;
	vls->config.max_tier1_voices = 32;
	vls->config.max_tier2_voices = 64;
	vls->config.max_tier3_voices = 160;
	vls->config.tier1_distance_m = 15.0f;
	vls->config.tier2_distance_m = 50.0f;
	vls->config.hysteresis_m = 1.5f;
	vls->config.occlusion_demote_db = 12.0f;
	vls->config.hoa_order = 2;
	vls->config.crossfade_time_ms = 10.0f;
	vls->sample_rate = sample_rate ? sample_rate : 48000;
	vls->enabled = true;
	vls->num_sources = 0;
}

void steamaudio_dsp_voice_lod_set_config(struct dsp_voice_lod_state *vls,
					 const struct sof_steamaudio_voice_lod_config *cfg)
{
	if (!vls || !cfg)
		return;

	vls->config = *cfg;
	if (vls->config.max_tier1_voices == 0)
		vls->config.max_tier1_voices = 32;
	if (vls->config.max_tier2_voices == 0)
		vls->config.max_tier2_voices = 64;
	if (vls->config.max_tier3_voices == 0)
		vls->config.max_tier3_voices = 160;
	if (vls->config.tier1_distance_m <= 0.0f)
		vls->config.tier1_distance_m = 15.0f;
	if (vls->config.tier2_distance_m <= vls->config.tier1_distance_m)
		vls->config.tier2_distance_m = vls->config.tier1_distance_m + 35.0f;
	if (vls->config.hysteresis_m < 0.0f)
		vls->config.hysteresis_m = 1.5f;

	vls->enabled = (cfg->enabled != 0);
}

struct dsp_lod_sort_item {
	uint32_t index;
	float score;
	uint8_t candidate_tier;
};

static void lod_bubble_sort_descending(struct dsp_lod_sort_item *items, uint32_t count)
{
	for (uint32_t i = 0; i < count; i++) {
		for (uint32_t j = i + 1; j < count; j++) {
			if (items[j].score > items[i].score) {
				struct dsp_lod_sort_item tmp = items[i];
				items[i] = items[j];
				items[j] = tmp;
			}
		}
	}
}

void steamaudio_dsp_voice_lod_classify(struct dsp_voice_lod_state *vls,
				       const struct raw_scene_packet *scene,
				       uint8_t *out_tiers)
{
	if (!vls || !scene)
		return;

	uint32_t count = scene->num_emitters;
	if (count > STEAMAUDIO_MAX_LOD_SOURCES)
		count = STEAMAUDIO_MAX_LOD_SOURCES;

	vls->num_sources = count;
	vls->stats.total_active_emitters = count;
	vls->stats.tier1_voice_count = 0;
	vls->stats.tier2_voice_count = 0;
	vls->stats.tier3_voice_count = 0;
	vls->stats.tier1_demotions = 0;
	vls->stats.tier2_demotions = 0;
	vls->stats.peak_voice_priority = 0.0f;

	if (!vls->enabled || count == 0) {
		for (uint32_t i = 0; i < count; i++) {
			if (out_tiers)
				out_tiers[i] = STEAMAUDIO_LOD_TIER_1;
			vls->sources[i].current_tier = STEAMAUDIO_LOD_TIER_1;
			vls->sources[i].target_tier = STEAMAUDIO_LOD_TIER_1;
		}
		vls->stats.tier1_voice_count = count;
		vls->stats.estimated_dsp_load_pct = (float)count * 1.5f;
		if (vls->stats.estimated_dsp_load_pct > 100.0f)
			vls->stats.estimated_dsp_load_pct = 100.0f;
		return;
	}

	float t1_dist = vls->config.tier1_distance_m;
	float t2_dist = vls->config.tier2_distance_m;
	float hyst = vls->config.hysteresis_m;
	float occ_demote_db = vls->config.occlusion_demote_db;
	float max_score = 0.0f;

	struct dsp_lod_sort_item sort_items[STEAMAUDIO_MAX_LOD_SOURCES];

	for (uint32_t i = 0; i < count; i++) {
		const struct raw_emitter_descriptor *e = &scene->emitters[i];
		float dx = e->pos[0] - scene->listener_pos[0];
		float dy = e->pos[1] - scene->listener_pos[1];
		float dz = e->pos[2] - scene->listener_pos[2];
		float dist = sqrtf(dx * dx + dy * dy + dz * dz);

		float spl = e->source_spl_db;
		if (spl < 0.0f) spl = 0.0f;
		float audibility = powf(10.0f, (spl - 90.0f) * 0.05f) / (dist > 1.0f ? dist : 1.0f);
		float occ = e->occlusion_factor;
		if (occ < 0.0f) occ = 0.0f;
		if (occ > 1.0f) occ = 1.0f;
		float score = audibility * (1.0f - 0.7f * occ);
		if (score > max_score)
			max_score = score;

		float trans_lin = 1.0f - occ;
		if (trans_lin < 1e-4f) trans_lin = 1e-4f;
		float occ_loss_db = -20.0f * log10f(trans_lin);

		uint8_t prev_tier = vls->sources[i].previous_tier;
		if (prev_tier == 0) prev_tier = STEAMAUDIO_LOD_TIER_1;

		uint8_t cand_tier;
		if (prev_tier == STEAMAUDIO_LOD_TIER_1) {
			if (dist < (t1_dist + hyst) && occ_loss_db < (occ_demote_db + 2.0f))
				cand_tier = STEAMAUDIO_LOD_TIER_1;
			else if (dist <= (t2_dist + hyst))
				cand_tier = STEAMAUDIO_LOD_TIER_2;
			else
				cand_tier = STEAMAUDIO_LOD_TIER_3;
		} else if (prev_tier == STEAMAUDIO_LOD_TIER_2) {
			if (dist < (t1_dist - hyst) && occ_loss_db < (occ_demote_db - 2.0f))
				cand_tier = STEAMAUDIO_LOD_TIER_1;
			else if (dist <= (t2_dist + hyst))
				cand_tier = STEAMAUDIO_LOD_TIER_2;
			else
				cand_tier = STEAMAUDIO_LOD_TIER_3;
		} else {
			if (dist < (t1_dist - hyst) && occ_loss_db < (occ_demote_db - 2.0f))
				cand_tier = STEAMAUDIO_LOD_TIER_1;
			else if (dist <= (t2_dist - hyst))
				cand_tier = STEAMAUDIO_LOD_TIER_2;
			else
				cand_tier = STEAMAUDIO_LOD_TIER_3;
		}

		sort_items[i].index = i;
		sort_items[i].score = score;
		sort_items[i].candidate_tier = cand_tier;

		vls->sources[i].source_id = e->source_id;
		vls->sources[i].distance_m = dist;
		vls->sources[i].priority_score = score;
		vls->sources[i].attenuation = audibility;
		vls->sources[i].active = 1;
	}

	vls->stats.peak_voice_priority = max_score;

	/* 1. Filter and sort candidate Tier 1 sources */
	struct dsp_lod_sort_item t1_candidates[STEAMAUDIO_MAX_LOD_SOURCES];
	uint32_t t1_cand_count = 0;
	for (uint32_t i = 0; i < count; i++) {
		if (sort_items[i].candidate_tier == STEAMAUDIO_LOD_TIER_1) {
			t1_candidates[t1_cand_count++] = sort_items[i];
		}
	}
	lod_bubble_sort_descending(t1_candidates, t1_cand_count);

	uint8_t final_tiers[STEAMAUDIO_MAX_LOD_SOURCES];
	memset(final_tiers, 0, sizeof(final_tiers));

	uint32_t max_t1 = vls->config.max_tier1_voices;
	for (uint32_t k = 0; k < t1_cand_count; k++) {
		uint32_t orig_idx = t1_candidates[k].index;
		if (k < max_t1) {
			final_tiers[orig_idx] = STEAMAUDIO_LOD_TIER_1;
		} else {
			/* Demote excess Tier 1 to Tier 2 */
			final_tiers[orig_idx] = STEAMAUDIO_LOD_TIER_2;
			vls->stats.tier1_demotions++;
		}
	}

	/* 2. Filter and sort candidate Tier 2 sources (including demoted from Tier 1) */
	struct dsp_lod_sort_item t2_candidates[STEAMAUDIO_MAX_LOD_SOURCES];
	uint32_t t2_cand_count = 0;
	for (uint32_t i = 0; i < count; i++) {
		if (final_tiers[i] == STEAMAUDIO_LOD_TIER_2 ||
		    (final_tiers[i] == 0 && sort_items[i].candidate_tier == STEAMAUDIO_LOD_TIER_2)) {
			t2_candidates[t2_cand_count++] = sort_items[i];
		}
	}
	lod_bubble_sort_descending(t2_candidates, t2_cand_count);

	uint32_t max_t2 = vls->config.max_tier2_voices;
	for (uint32_t k = 0; k < t2_cand_count; k++) {
		uint32_t orig_idx = t2_candidates[k].index;
		if (k < max_t2) {
			final_tiers[orig_idx] = STEAMAUDIO_LOD_TIER_2;
		} else {
			/* Demote excess Tier 2 to Tier 3 */
			final_tiers[orig_idx] = STEAMAUDIO_LOD_TIER_3;
			vls->stats.tier2_demotions++;
		}
	}

	/* 3. Assign remaining sources to Tier 3 */
	for (uint32_t i = 0; i < count; i++) {
		if (final_tiers[i] == 0) {
			final_tiers[i] = STEAMAUDIO_LOD_TIER_3;
		}
	}

	/* Store results into vls state and output array */
	uint32_t t1_cnt = 0, t2_cnt = 0, t3_cnt = 0;
	for (uint32_t i = 0; i < count; i++) {
		uint8_t tier = final_tiers[i];
		if (out_tiers)
			out_tiers[i] = tier;

		vls->sources[i].target_tier = tier;
		vls->sources[i].previous_tier = vls->sources[i].current_tier;
		vls->sources[i].current_tier = tier;

		if (tier == STEAMAUDIO_LOD_TIER_1) t1_cnt++;
		else if (tier == STEAMAUDIO_LOD_TIER_2) t2_cnt++;
		else t3_cnt++;
	}

	vls->stats.tier1_voice_count = t1_cnt;
	vls->stats.tier2_voice_count = t2_cnt;
	vls->stats.tier3_voice_count = t3_cnt;

	float est_load = (float)t1_cnt * 1.5f + (float)t2_cnt * 0.3f + (float)t3_cnt * 0.08f;
	if (est_load > 100.0f) est_load = 100.0f;
	vls->stats.estimated_dsp_load_pct = est_load;
}

void steamaudio_dsp_voice_lod_process(struct steamaudio_comp_data *cd,
				      const struct raw_scene_packet *scene,
				      const float *in_pcm,
				      float *out_l,
				      float *out_r,
				      uint32_t frames,
				      bool muted)
{
	if (!cd || !out_l || !out_r || frames == 0)
		return;

	if (frames > 256)
		frames = 256;

	memset(out_l, 0, frames * sizeof(float));
	memset(out_r, 0, frames * sizeof(float));

	if (!scene || scene->num_emitters == 0 || !in_pcm)
		return;

	uint32_t count = scene->num_emitters;
	if (count > STEAMAUDIO_MAX_LOD_SOURCES)
		count = STEAMAUDIO_MAX_LOD_SOURCES;

	uint8_t tiers[STEAMAUDIO_MAX_LOD_SOURCES];
	steamaudio_dsp_voice_lod_classify(&cd->voice_lod, scene, tiers);

	/* Clear Tier 2 HOA bed (9 channels) and Tier 3 diffuse bed */
	memset(cd->voice_lod.hoa_bed, 0, sizeof(cd->voice_lod.hoa_bed));
	memset(cd->voice_lod.diffuse_bed, 0, sizeof(cd->voice_lod.diffuse_bed));

	for (uint32_t i = 0; i < count; i++) {
		const float *src_pcm = in_pcm + i * frames;
		uint8_t tier = muted ? STEAMAUDIO_LOD_TIER_1 : tiers[i];
		const struct raw_emitter_descriptor *e = &scene->emitters[i];

		float dx = e->pos[0] - scene->listener_pos[0];
		float dy = e->pos[1] - scene->listener_pos[1];
		float dz = e->pos[2] - scene->listener_pos[2];
		float dist = sqrtf(dx * dx + dy * dy + dz * dz);
		float inv_d = (dist > 1.0f) ? (1.0f / dist) : 1.0f;
		float gain = inv_d * (1.0f - 0.5f * e->occlusion_factor);

		if (tier == STEAMAUDIO_LOD_TIER_1) {
			/* Tier 1: Near-Field / Critical -> Full Spatial rendering */
			float wx = (dist > 1e-6f) ? (dx / dist) : 0.0f;
			float wy = (dist > 1e-6f) ? (dy / dist) : 0.0f;
			float wz = (dist > 1e-6f) ? (dz / dist) : 1.0f;
			float lx = scene->listener_rotation[0][0] * wx + scene->listener_rotation[1][0] * wy + scene->listener_rotation[2][0] * wz;

			float pan_l = 0.5f * (1.0f - lx);
			float pan_r = 0.5f * (1.0f + lx);

			for (uint32_t n = 0; n < frames; n++) {
				float s = src_pcm[n] * gain;
				out_l[n] += s * pan_l;
				out_r[n] += s * pan_r;
			}
		} else if (tier == STEAMAUDIO_LOD_TIER_2) {
			/* Tier 2: Mid-Field -> 2nd-Order Ambisonics (HOA) Binning (9 channels) */
			float wx = (dist > 1e-6f) ? (dx / dist) : 0.0f;
			float wy = (dist > 1e-6f) ? (dy / dist) : 0.0f;
			float wz = (dist > 1e-6f) ? (dz / dist) : 1.0f;
			float lx = scene->listener_rotation[0][0] * wx + scene->listener_rotation[1][0] * wy + scene->listener_rotation[2][0] * wz;
			float ly = scene->listener_rotation[0][1] * wx + scene->listener_rotation[1][1] * wy + scene->listener_rotation[2][1] * wz;
			float lz = scene->listener_rotation[0][2] * wx + scene->listener_rotation[1][2] * wy + scene->listener_rotation[2][2] * wz;

			/* Spherical harmonics for 2nd order */
			float y0 = 0.282095f;
			float y1 = 0.488603f * ly;
			float y2 = 0.488603f * lz;
			float y3 = 0.488603f * lx;
			float y4 = 1.092548f * lx * ly;
			float y5 = 1.092548f * ly * lz;
			float y6 = 0.315392f * (3.0f * lz * lz - 1.0f);
			float y7 = 1.092548f * lx * lz;
			float y8 = 0.546274f * (lx * lx - ly * ly);

			float y[9] = { y0, y1, y2, y3, y4, y5, y6, y7, y8 };
			for (int ch = 0; ch < 9; ch++) {
				float ch_gain = gain * y[ch];
				for (uint32_t n = 0; n < frames; n++) {
					cd->voice_lod.hoa_bed[ch][n] += src_pcm[n] * ch_gain;
				}
			}
		} else {
			/* Tier 3: Far-Field -> Diffuse Energy Field Accumulator */
			for (uint32_t n = 0; n < frames; n++) {
				float s = src_pcm[n] * gain * 0.7071f;
				cd->voice_lod.diffuse_bed[0][n] += s;
				cd->voice_lod.diffuse_bed[1][n] += s;
			}
		}
	}

	/* Decode Tier 2 HOA bed to stereo */
	for (uint32_t n = 0; n < frames; n++) {
		float w = cd->voice_lod.hoa_bed[0][n];
		float y = cd->voice_lod.hoa_bed[1][n];
		float x = cd->voice_lod.hoa_bed[3][n];
		out_l[n] += 0.7071f * (w + 0.7071f * (x - y));
		out_r[n] += 0.7071f * (w + 0.7071f * (x + y));
	}

	/* Add Tier 3 Diffuse bed */
	for (uint32_t n = 0; n < frames; n++) {
		out_l[n] += cd->voice_lod.diffuse_bed[0][n];
		out_r[n] += cd->voice_lod.diffuse_bed[1][n];
	}

	/* Soft saturation protection */
	for (uint32_t n = 0; n < frames; n++) {
		if (out_l[n] > 1.0f) out_l[n] = 1.0f;
		else if (out_l[n] < -1.0f) out_l[n] = -1.0f;
		if (out_r[n] > 1.0f) out_r[n] = 1.0f;
		else if (out_r[n] < -1.0f) out_r[n] = -1.0f;
	}
}

/* Scene-Aware 5.1 / 7.1 Acoustic Upmixer Implementation */

static void steamaudio_dsp_upmix_compute_crossover_coeffs(struct dsp_upmix_state *ums)
{
	float fc = ums->config.crossover_freq_hz;
	if (fc < 40.0f) fc = 40.0f;
	if (fc > 300.0f) fc = 300.0f;

	float fs = (float)ums->sample_rate;
	if (fs < 8000.0f) fs = 48000.0f;

	float w0 = 2.0f * 3.14159265f * fc / fs;
	if (w0 < 0.001f) w0 = 0.001f;
	if (w0 > 1.5f) w0 = 1.5f;

	float cos_w0 = cosf(w0);
	float sin_w0 = sinf(w0);
	/* 2nd-order Butterworth has Q = 1 / sqrt(2) = 0.70710678f */
	float alpha = sin_w0 * 0.70710678f;

	/* Butterworth LPF coefficients */
	float b0_lpf = (1.0f - cos_w0) * 0.5f;
	float b1_lpf = 1.0f - cos_w0;
	float b2_lpf = (1.0f - cos_w0) * 0.5f;
	float a0_lpf = 1.0f + alpha;
	float a1_lpf = -2.0f * cos_w0;
	float a2_lpf = 1.0f - alpha;

	/* Butterworth HPF coefficients */
	float b0_hpf = (1.0f + cos_w0) * 0.5f;
	float b1_hpf = -(1.0f + cos_w0);
	float b2_hpf = (1.0f + cos_w0) * 0.5f;
	float a0_hpf = 1.0f + alpha;
	float a1_hpf = -2.0f * cos_w0;
	float a2_hpf = 1.0f - alpha;

	for (int spk = 0; spk < STEAMAUDIO_MAX_SPEAKERS; spk++) {
		for (int stg = 0; stg < 2; stg++) {
			ums->lpf_coeffs[spk][stg][0] = b0_lpf / a0_lpf;
			ums->lpf_coeffs[spk][stg][1] = b1_lpf / a0_lpf;
			ums->lpf_coeffs[spk][stg][2] = b2_lpf / a0_lpf;
			ums->lpf_coeffs[spk][stg][3] = a1_lpf / a0_lpf;
			ums->lpf_coeffs[spk][stg][4] = a2_lpf / a0_lpf;

			ums->hpf_coeffs[spk][stg][0] = b0_hpf / a0_hpf;
			ums->hpf_coeffs[spk][stg][1] = b1_hpf / a0_hpf;
			ums->hpf_coeffs[spk][stg][2] = b2_hpf / a0_hpf;
			ums->hpf_coeffs[spk][stg][3] = a1_hpf / a0_hpf;
			ums->hpf_coeffs[spk][stg][4] = a2_hpf / a0_hpf;
		}
	}
}

static inline float biquad_process_df2t(const float c[5], float s[2], float in)
{
	/* c: b0, b1, b2, a1, a2 */
	float out = c[0] * in + s[0];
	s[0] = c[1] * in - c[3] * out + s[1];
	s[1] = c[2] * in - c[4] * out;
	return out;
}

void steamaudio_dsp_upmix_init(struct dsp_upmix_state *ums, uint32_t layout_type, uint32_t sample_rate)
{
	if (!ums)
		return;

	memset(ums, 0, sizeof(*ums));
	ums->config.comp_type = STEAMAUDIO_PARAM_SCENE_UPMIX;
	ums->config.layout_type = (layout_type == STEAMAUDIO_SPEAKER_LAYOUT_5_1) ?
				  STEAMAUDIO_SPEAKER_LAYOUT_5_1 : STEAMAUDIO_SPEAKER_LAYOUT_7_1;
	ums->config.center_spread = 0.0f;
	ums->config.center_threshold_rad = 0.2618f; /* 15 degrees */
	ums->config.ambient_decorrelation = 0.7071f;
	ums->config.reverb_surround_mix = 1.0f;
	ums->config.crossover_freq_hz = 80.0f;
	ums->config.enable_bvh_reflections = 1;
	ums->config.enable_bass_management = 1;

	ums->num_speakers = (ums->config.layout_type == STEAMAUDIO_SPEAKER_LAYOUT_5_1) ? 6 : 8;
	ums->sample_rate = sample_rate ? sample_rate : 48000;
	ums->enabled = true;

	/* Prime delays in samples scaled to sample rate */
	float rate_scale = (float)ums->sample_rate / 48000.0f;
	ums->ap_delays[0] = (uint32_t)(341.0f * rate_scale);
	ums->ap_delays[1] = (uint32_t)(541.0f * rate_scale);
	ums->ap_delays[2] = (uint32_t)(659.0f * rate_scale);
	ums->ap_delays[3] = (uint32_t)(859.0f * rate_scale);

	for (int i = 0; i < 4; i++) {
		if (ums->ap_delays[i] >= STEAMAUDIO_UPMIX_DECORR_DELAY_MAX)
			ums->ap_delays[i] = STEAMAUDIO_UPMIX_DECORR_DELAY_MAX - 1;
		if (ums->ap_delays[i] == 0)
			ums->ap_delays[i] = 1;
	}

	steamaudio_dsp_upmix_compute_crossover_coeffs(ums);
}

void steamaudio_dsp_upmix_set_config(struct dsp_upmix_state *ums,
				     const struct sof_steamaudio_upmix_config *cfg)
{
	if (!ums || !cfg)
		return;

	float prev_fc = ums->config.crossover_freq_hz;
	ums->config = *cfg;
	ums->config.comp_type = STEAMAUDIO_PARAM_SCENE_UPMIX;

	if (ums->config.layout_type != STEAMAUDIO_SPEAKER_LAYOUT_5_1 &&
	    ums->config.layout_type != STEAMAUDIO_SPEAKER_LAYOUT_7_1) {
		ums->config.layout_type = STEAMAUDIO_SPEAKER_LAYOUT_7_1;
	}
	ums->num_speakers = (ums->config.layout_type == STEAMAUDIO_SPEAKER_LAYOUT_5_1) ? 6 : 8;

	if (ums->config.center_spread < 0.0f) ums->config.center_spread = 0.0f;
	if (ums->config.center_spread > 1.0f) ums->config.center_spread = 1.0f;

	if (ums->config.center_threshold_rad < 0.05f) ums->config.center_threshold_rad = 0.05f;
	if (ums->config.center_threshold_rad > 1.0f) ums->config.center_threshold_rad = 1.0f;

	if (ums->config.ambient_decorrelation < 0.0f) ums->config.ambient_decorrelation = 0.0f;
	if (ums->config.ambient_decorrelation > 2.0f) ums->config.ambient_decorrelation = 2.0f;

	if (ums->config.reverb_surround_mix < 0.0f) ums->config.reverb_surround_mix = 0.0f;
	if (ums->config.reverb_surround_mix > 3.0f) ums->config.reverb_surround_mix = 3.0f;

	if (ums->config.crossover_freq_hz != prev_fc) {
		steamaudio_dsp_upmix_compute_crossover_coeffs(ums);
	}
}

void steamaudio_dsp_upmix_process(struct steamaudio_comp_data *cd,
				  const float *in_stereo_l,
				  const float *in_stereo_r,
				  float out_channels[STEAMAUDIO_MAX_SPEAKERS][256],
				  uint32_t frames,
				  bool muted)
{
	if (!cd || !in_stereo_l || !in_stereo_r || !out_channels || frames == 0)
		return;

	struct dsp_upmix_state *ums = &cd->upmix;
	uint32_t num_spk = ums->num_speakers;

	if (muted || !ums->enabled) {
		for (uint32_t n = 0; n < frames; n++) {
			out_channels[0][n] = in_stereo_l[n];
			out_channels[1][n] = in_stereo_r[n];
			for (uint32_t ch = 2; ch < STEAMAUDIO_MAX_SPEAKERS; ch++) {
				out_channels[ch][n] = 0.0f;
			}
		}
		return;
	}

	for (uint32_t ch = 0; ch < STEAMAUDIO_MAX_SPEAKERS; ch++) {
		for (uint32_t n = 0; n < frames; n++) {
			out_channels[ch][n] = 0.0f;
		}
	}

	float c_spread = ums->config.center_spread;
	float amb_gain = ums->config.ambient_decorrelation;
	const float g_allpass = 0.618034f; /* Golden ratio all-pass reflection coefficient */

	/* Temporary sub-bass buffer for Linkwitz-Riley LFE extraction */
	float sub_bass[256];
	memset(sub_bass, 0, frames * sizeof(float));

	/* Sub-Engine 2: Stereo Bed Direct/Diffuse M/S Decomposition & Decorrelation */
	for (uint32_t n = 0; n < frames; n++) {
		float left_val = in_stereo_l[n];
		float right_val = in_stereo_r[n];

		/* Mid (correlated, direct) and Side (uncorrelated, ambient) */
		float mid = (left_val + right_val) * 0.70710678f;
		float side = (left_val - right_val) * 0.70710678f;

		/* Center Channel Anchoring */
		float c_bed = mid * (1.0f - c_spread) * 0.70710678f;
		out_channels[2][n] += c_bed;

		/* Front Left and Front Right receive remaining front energy */
		out_channels[0][n] += left_val - c_bed * 0.5f;
		out_channels[1][n] += right_val - c_bed * 0.5f;

		/* Schroeder 4-channel prime-delay all-pass decorrelators for Side component */
		float y_ap[4];
		for (int k = 0; k < 4; k++) {
			int r_idx = (int)ums->ap_write_idx[k] - (int)ums->ap_delays[k];
			if (r_idx < 0)
				r_idx += STEAMAUDIO_UPMIX_DECORR_DELAY_MAX;

			float delayed_v = ums->ap_buffers[k][r_idx];
			float y_val = delayed_v - g_allpass * side;
			ums->ap_buffers[k][ums->ap_write_idx[k]] = side + g_allpass * y_val;
			ums->ap_write_idx[k] = (ums->ap_write_idx[k] + 1) % STEAMAUDIO_UPMIX_DECORR_DELAY_MAX;
			y_ap[k] = y_val;
		}

		/* Route decorrelated ambience to surround speakers */
		if (num_spk == 8) {
			/* 7.1 Layout: Side Left (6), Side Right (7), Rear Left (4), Rear Right (5) */
			out_channels[6][n] += y_ap[0] * amb_gain;
			out_channels[7][n] += -y_ap[1] * amb_gain; /* Anti-phase for expansive lateral width */
			out_channels[4][n] += y_ap[2] * amb_gain;
			out_channels[5][n] += y_ap[3] * amb_gain;
		} else {
			/* 5.1 Layout: Combine into Rear Left (4) and Rear Right (5) */
			out_channels[4][n] += (0.7071f * y_ap[0] + 0.5f * y_ap[2]) * amb_gain;
			out_channels[5][n] += (-0.7071f * y_ap[1] + 0.5f * y_ap[3]) * amb_gain;
		}
	}

	/* Sub-Engine 4: 8-Channel FDN Diffuse Reverb Surround Expansion */
	float rev_surround = ums->config.reverb_surround_mix;
	if (cd->reverb.wet_gain > 1e-4f && !(cd->mute_mask & ((1u << STEAMAUDIO_STEP_REVERB) | (1u << STEAMAUDIO_STEP_CONVOLUTION)))) {
		float wet = cd->reverb.wet_gain;
		for (uint32_t n = 0; n < frames; n++) {
			/* Each FDN delay buffer output directly maps to a physical surround channel */
			float d0 = cd->reverb.delay_buffers[0][(cd->reverb.delay_indices[0] + n) % cd->reverb.delay_lengths[0]];
			float d1 = cd->reverb.delay_buffers[1][(cd->reverb.delay_indices[1] + n) % cd->reverb.delay_lengths[1]];
			float d2 = cd->reverb.delay_buffers[2][(cd->reverb.delay_indices[2] + n) % cd->reverb.delay_lengths[2]];
			float d3 = cd->reverb.delay_buffers[3][(cd->reverb.delay_indices[3] + n) % cd->reverb.delay_lengths[3]];
			float d4 = cd->reverb.delay_buffers[4][(cd->reverb.delay_indices[4] + n) % cd->reverb.delay_lengths[4]];
			float d5 = cd->reverb.delay_buffers[5][(cd->reverb.delay_indices[5] + n) % cd->reverb.delay_lengths[5]];
			float d6 = cd->reverb.delay_buffers[6][(cd->reverb.delay_indices[6] + n) % cd->reverb.delay_lengths[6]];
			float d7 = cd->reverb.delay_buffers[7][(cd->reverb.delay_indices[7] + n) % cd->reverb.delay_lengths[7]];

			out_channels[0][n] += d0 * wet * 0.25f;
			out_channels[1][n] += d1 * wet * 0.25f;
			out_channels[2][n] += d2 * wet * 0.125f; /* Attenuated for center dialogue clarity */
			out_channels[3][n] += d3 * wet * 0.125f; /* Low reverb in LFE */
			out_channels[4][n] += d4 * wet * 0.25f * rev_surround;
			out_channels[5][n] += d5 * wet * 0.25f * rev_surround;

			if (num_spk == 8) {
				out_channels[6][n] += d6 * wet * 0.25f * rev_surround;
				out_channels[7][n] += d7 * wet * 0.25f * rev_surround;
			} else {
				out_channels[4][n] += d6 * wet * 0.125f * rev_surround;
				out_channels[5][n] += d7 * wet * 0.125f * rev_surround;
			}
		}
	}

	/* Sub-Engine 5: 4th-Order Linkwitz-Riley Crossover & Bass Management */
	if (ums->config.enable_bass_management) {
		for (uint32_t ch = 0; ch < num_spk; ch++) {
			if (ch == 3) /* Skip LFE channel itself */
				continue;

			for (uint32_t n = 0; n < frames; n++) {
				float s_in = out_channels[ch][n];

				/* 2 cascaded 2nd-order Butterworth LPF stages -> 4th-order Linkwitz-Riley LPF */
				float lpf_stg1 = biquad_process_df2t(ums->lpf_coeffs[ch][0], ums->lpf_states[ch][0], s_in);
				float lpf_stg2 = biquad_process_df2t(ums->lpf_coeffs[ch][1], ums->lpf_states[ch][1], lpf_stg1);
				sub_bass[n] += lpf_stg2;

				/* 2 cascaded 2nd-order Butterworth HPF stages -> 4th-order Linkwitz-Riley HPF */
				float hpf_stg1 = biquad_process_df2t(ums->hpf_coeffs[ch][0], ums->hpf_states[ch][0], s_in);
				float hpf_stg2 = biquad_process_df2t(ums->hpf_coeffs[ch][1], ums->hpf_states[ch][1], hpf_stg1);
				out_channels[ch][n] = hpf_stg2;
			}
		}

		/* Sum sub-bass from all satellites into LFE channel */
		for (uint32_t n = 0; n < frames; n++) {
			out_channels[3][n] += sub_bass[n] * 0.5f;
		}
	}

	/* Soft saturation protection across all channels */
	for (uint32_t ch = 0; ch < STEAMAUDIO_MAX_SPEAKERS; ch++) {
		for (uint32_t n = 0; n < frames; n++) {
			if (out_channels[ch][n] > 1.0f) out_channels[ch][n] = 1.0f;
			else if (out_channels[ch][n] < -1.0f) out_channels[ch][n] = -1.0f;
		}
	}
}









