// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

/* Topology parser */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <ipc/topology.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/ipc/topology.h>
#include <kernel/header.h>
#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>

/* load pipeline graph DAPM widget*/
int tplg_create_graph(struct tplg_context *ctx, int count, int pipeline_id,
		      struct tplg_comp_info *temp_comp_list, char *pipeline_string,
		      struct sof_ipc_pipe_comp_connect *connection,
		      int route_num)
{
	struct snd_soc_tplg_dapm_graph_elem *graph_elem;
	char *source = NULL, *sink = NULL;
	int j;

	/* configure route */
	connection->hdr.size = sizeof(*connection);
	connection->hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_CONNECT;


	/* set up component connections */
	connection->source_id = -1;
	connection->sink_id = -1;

	graph_elem = tplg_get_graph(ctx);

	/* look up component id from the component list */
	for (j = 0; j < count; j++) {
		if (strcmp(temp_comp_list[j].name,
			   graph_elem->source) == 0) {
			connection->source_id = temp_comp_list[j].id;
			source = graph_elem->source;
		}

		if (strcmp(temp_comp_list[j].name,
			   graph_elem->sink) == 0) {
			connection->sink_id = temp_comp_list[j].id;
			sink = graph_elem->sink;
		}
	}

	if (!source || !sink) {
		fprintf(stderr, "%s() error: source=%p, sink=%p\n",
			__func__, source, sink);
		return -EINVAL;
	}

	printf("loading route %s -> %s\n", source, sink);

	strcat(pipeline_string, graph_elem->source);
	strcat(pipeline_string, "->");

	if (route_num == (count - 1)) {
		strcat(pipeline_string, graph_elem->sink);
		strcat(pipeline_string, "\n");
	}

	return 0;
}

/* parse and save the route information for IPC4 */
int tplg_parse_graph(struct tplg_context *ctx, struct list_item *widget_list,
		     struct list_item *route_list)
{
	struct snd_soc_tplg_dapm_graph_elem *graph_elem;
	struct tplg_route_info *route;
	struct list_item *item;

	route = calloc(sizeof(struct tplg_route_info), 1);
	if (!route)
		return -ENOMEM;

	graph_elem = tplg_get_graph(ctx);

	/* look up from the widget list and populate the route info */
	list_for_item(item, widget_list) {
		struct tplg_comp_info *comp_info = container_of(item, struct tplg_comp_info, item);

		if (!strcmp(comp_info->name, graph_elem->source))
			route->source = comp_info;

		if (!strcmp(comp_info->name, graph_elem->sink))
			route->sink = comp_info;
	}

	if (!route->source || !route->sink) {
		fprintf(stderr, "%s() error loading route: source=%s, sink=%s\n",
			__func__, graph_elem->source, graph_elem->sink);
		free(route);
		return -EINVAL;
	}

	list_item_append(&route->item, route_list);

	tplg_debug("loading route %s -> %s\n", route->source->name, route->sink->name);

	return 0;
}
