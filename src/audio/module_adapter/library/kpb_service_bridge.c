// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2026 Intel Corporation. All rights reserved.

/*
 * It is better to keep most of the implementation in C because SOF C headers
 * do not provide extern "C" blocks; C++ is used as a wrapper.
 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/audio/kpb.h>
#include <sof/audio/module_adapter/library/kpb_service.h>

struct kpb_service_client {
	struct kpb_client client;
	size_t current_history_depth;
	size_t max_history_depth;
	bool in_use;
};

static APP_TASK_DATA struct kpb_service_client clients[KPB_MAX_NO_OF_CLIENTS];

static bool kpb_service_client_is_valid(const struct kpb_service_client *client)
{
	int i;

	for (i = 0; i < KPB_MAX_NO_OF_CLIENTS; i++) {
		if (client == &clients[i])
			return clients[i].in_use;
	}

	return false;
}

static AdspErrorCode kpb_service_request_draining(struct kpb_service_client *client)
{
	switch (kpb_request_draining(client->client.id, client->client.drain_req)) {
	case 0:
		return ADSP_NO_ERROR;
	case -ENODEV:
		return ADSP_SERVICE_UNAVAILABLE;
	default:
		return ADSP_INVALID_PARAMETERS;
	}
}

AdspErrorCode kpb_service_register_client(size_t current_history_depth,
					  size_t max_history_depth, uint32_t output_pin,
					  struct kpb_service_client **client)
{
	struct kpb_service_client *new_client;

	if (!client || output_pin >= KPB_MAX_NO_OF_CLIENTS)
		return ADSP_INVALID_PARAMETERS;

	max_history_depth = MIN(max_history_depth, (size_t)KPB_MAX_DRAINING_REQ);
	current_history_depth = MIN(current_history_depth, max_history_depth);

	*client = NULL;
	new_client = &clients[output_pin];
	if (new_client->in_use)
		return ADSP_BUSY_RESOURCE;

	new_client->in_use = true;
	new_client->current_history_depth = current_history_depth;
	new_client->max_history_depth = max_history_depth;
	new_client->client.id = output_pin;
	new_client->client.drain_req = current_history_depth;
	new_client->client.state = KPB_CLIENT_UNREGISTERED;
	new_client->client.r_ptr = NULL;
	new_client->client.sink = NULL;

	*client = new_client;
	return ADSP_NO_ERROR;
}

AdspErrorCode kpb_service_signal_detection(struct kpb_service_client *client,
					   uint32_t phrase_length)
{
	if (!kpb_service_client_is_valid(client))
		return ADSP_INVALID_PARAMETERS;

	client->client.drain_req = MIN(client->current_history_depth + phrase_length,
				       client->max_history_depth);
	return kpb_service_request_draining(client);
}

AdspErrorCode kpb_service_unregister_client(struct kpb_service_client *client)
{
	if (!kpb_service_client_is_valid(client))
		return ADSP_INVALID_PARAMETERS;

	client->in_use = false;
	return ADSP_NO_ERROR;
}
