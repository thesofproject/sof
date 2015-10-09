/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_STREAM__
#define __INCLUDE_STREAM__

#define STREAM_DIR_PLAYBACK	(1 << 0)
#define STREAM_DIR_CAPTURE	(1 << 1)

struct stream {

};

struct stream_new(int fe_id);

int stream_trigger(struct stream *stream, int direction);

int stream_params(struct stream *stream, int direction,
	int rate, int channels, int format, struct chmap *map);

#endif
