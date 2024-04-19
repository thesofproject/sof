// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>
#include <adsp_debug_window.h>
#include <adsp_memory.h>
#include <sof/common.h>
#include "telemetry2_slot.h"

LOG_MODULE_REGISTER(telemetry2_slot);

static struct k_spinlock lock;

static
struct telemetry2_chunk_hdr *telemetry2_chunk_search(struct telemetry2_chunk_hdr *chunk,
						     int id)
{
	uint8_t *p;

	/* Loop through the chunks and look for 'id'. If found leave a
	 * pointer to it in 'chunk', if not leave 'chunk' pointing at
	 * the beginning of the free space.
	 */
	while (chunk->id != id && chunk->size != 0) {
		p = (((uint8_t *) chunk) + chunk->size);

		chunk = (struct telemetry2_chunk_hdr *) p;
	}
	return chunk;
}

/*
 * Search, or if not found reserve, a chunk of specified size and id from
 * telemetry2 slot. Each chuck must be aligned with a 64-byte cache line.
 * A pointer to the chunk is returned.
 */
void *telemetry2_chunk_get(int id, size_t required_size)
{
	const int slot = CONFIG_SOF_TELEMETRY2_SLOT_NUMBER;
	struct telemetry2_payload_hdr *payload =
		(struct telemetry2_payload_hdr *)(ADSP_DW->slots[slot]);
	size_t hdr_size = ALIGN_UP(sizeof(*payload), CONFIG_DCACHE_LINE_SIZE);
	struct telemetry2_chunk_hdr *chunk = (struct telemetry2_chunk_hdr *)
		(((uint8_t *) payload) + hdr_size);
	size_t size = ALIGN_UP(required_size, CONFIG_DCACHE_LINE_SIZE);
	k_spinlock_key_t key;

	if (ADSP_DW->descs[slot].type != ADSP_DW_SLOT_TELEMETRY2) {
		if (ADSP_DW->descs[slot].type != 0)
			LOG_WRN("Slot %d was not free: %u", slot, ADSP_DW->descs[slot].type);
		LOG_INF("Initializing telemetry2 slot 0x%08x", ADSP_DW->descs[slot].type);
		ADSP_DW->descs[slot].type = ADSP_DW_SLOT_TELEMETRY2;
		payload->hdr_size = hdr_size;
		payload->magic = TELEMETRY2_PAYLOAD_MAGIC;
		payload->abi = TELEMETRY2_PAYLOAD_V0_0;
	}

	LOG_INF("Add id %u size %u (after alignment %u)", id, required_size, size);
	chunk = telemetry2_chunk_search(chunk, id);
	if (id == chunk->id)
		goto match_found;

	/* End of chunk list reached, but specified chunk not found. We
	 * need to reseeve one, so take the spin lock and check if more
	 * chunks were added since the previus search.
	 */
	key = k_spin_lock(&lock);
	chunk = telemetry2_chunk_search(chunk, id);
	/* Check if some other thread beat us to add the chunk we want */
	if (id == chunk->id) {
		k_spin_unlock(&lock, key);
		goto match_found;
	}

	/* If the chunk was not found, reserve space for it after
	 * the last reserved chunk.
	 */
	if (((uint8_t *) chunk) + size >= ADSP_DW->slots[slot + 1]) {
		LOG_ERR("No space for chunk %u of size %u in slot %u, offset %u",
			id, size, slot, ((uint8_t *) chunk) - ADSP_DW->slots[slot]);
		k_spin_unlock(&lock, key);
		return NULL;
	}

	if (chunk->id != TELEMETRY2_CHUNK_ID_EMPTY)
		LOG_WRN("Chunk of size %u has type %u, assuming empty",
			chunk->size, chunk->id);

	LOG_INF("Chunk %u reserved", id);
	chunk->size = size;
	chunk->id = id;
	payload->total_size = size + ((uint8_t *) chunk) - ((uint8_t *) payload);

	k_spin_unlock(&lock, key);

	return chunk;

match_found:
	/* If a matching chunk was found check that the size is enough
	 * for what was requested. If yes return the value, if not
	 * return NULL.
	 */
	if (required_size > chunk->size) {
		LOG_ERR("Chunk %d size too small %u > %u",
			id, required_size, chunk->size);
		return NULL;
	}
	LOG_INF("Chunk %u found", id);
	return chunk;
}
