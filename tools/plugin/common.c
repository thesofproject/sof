/*-*- linux-c -*-*/

/*
 * ALSA <-> SOF PCM I/O plugin
 *
 * Copyright (c) 2022 by Liam Girdwood <liam.r.girdwood@intel.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "common.h"

/*
 * Timing
 */
void plug_timespec_add_ms(struct timespec *ts, unsigned long ms)
{
	long ns;
	long secs = ms / 1000;

	/* get ms remainder */
	ms = ms - (secs * 1000);
	ns = ms * 1000000;

	ts->tv_nsec += ns;
	if (ts->tv_nsec > 1000000000) {
		secs++;
		ts->tv_nsec -= 1000000000;
	}
	ts->tv_sec += (secs + DEBUG_TV_SECS);
}

long plug_timespec_delta_ns(struct timespec *before, struct timespec *after)
{
	long ns;

	ns = (after->tv_sec - before->tv_sec) * 1000000000;
	ns += after->tv_nsec - before->tv_nsec;

	return ns;
}

static const char *suffix_name(const char *longname)
{
	size_t len = strlen(longname);
	int i = len;

	/* longname name invalid */
	if (len < 1) {
		SNDERR("invalid topology long name\n");
		return NULL;
	}

	/* find the last '/' in the longname topology path */
	while (--i >= 0) {
		if (longname[i] == '/') {
			i += 1; /* skip / */
			return &longname[i];
		}
	}

	/* no / in topology path, so use full path */
	return longname;
}

/*
 * IPC
 *
 * POSIX message queues are used for interprocess IPC messaging.
 */

/*
 * Initialise the IPC object.
 */
int plug_socket_path_init(struct plug_socket_desc *ipc, const char *tplg, const char *type,
			  int index)
{
	snprintf(ipc->path, NAME_SIZE, "/tmp/%s-%s", tplg, type);
	return 0;
}

/*
 * Locking
 *
 * POSIX semaphores are used to block and synchronise audio between
 * different threads and processes.
 */

/*
 * Initialise the lock object.
 */
int plug_lock_init(struct plug_sem_desc *lock, const char *tplg, const char *type, int index)
{
	const char *name = suffix_name(tplg);

	if (!name)
		return -EINVAL;

	/* semaphores need the leading / */
	snprintf(lock->name, NAME_SIZE, "/lock-%s-%s-%d", name, type, index);

	return 0;
}

/*
 * SHM
 *
 * Shared memory is used for audio data and audio context sharing between
 * threads and processes.
 */

/*
 * Initialise the SHM object.
 */
int plug_shm_init(struct plug_shm_desc *shm, const char *tplg, const char *type, int index)
{
	const char *name = suffix_name(tplg);

	if (!name)
		return -EINVAL;

	snprintf(shm->name, NAME_SIZE, "/shm-%s-%s-%d", name, type, index);
	shm->size = SHM_SIZE;

	return 0;
}

/*
 * Open an existing shared memory region using the SHM object.
 */
int plug_shm_open(struct plug_shm_desc *shm)
{
	struct stat status;

	/* open SHM to be used for low latency position */
	shm->fd = shm_open(shm->name, O_RDWR,
			   S_IRWXU | S_IRWXG);
	if (shm->fd < 0) {
		//SNDERR("failed to open SHM position %s: %s\n",
		//	shm->name, strerror(errno));
		return -errno;
	}

	fstat(shm->fd, &status);
	/* map it locally for context readback */
	shm->addr = mmap(NULL, status.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
	if (!shm->addr) {
		SNDERR("failed to mmap SHM position%s: %s\n", shm->name, strerror(errno));
		return -errno;
	}

	return 0;
}

static int plug_socket_timed_wait(struct plug_socket_desc *ipc, fd_set *fds, int timeout_ms,
				  bool write)
{
	struct timeval timeout;
	int result;

	/* Set the timeout for select */
	timeout.tv_sec = 0;
	timeout.tv_usec = timeout_ms * 1000;

	/* now wait for socket to be readable/writable */
	if (write)
		result = select(ipc->socket_fd + 1, NULL, fds, NULL, &timeout);
	else
		result = select(ipc->socket_fd + 1, fds, NULL, NULL, &timeout);

	if (result == -1) {
		SNDERR("error waiting for socket to be %s\n", write ? "writable" : "readable");
		return result;
	}

	if (result == 0) {
		SNDERR("IPC Socket %s timeout\n", write ? "write" : "read");
		return -ETIMEDOUT;
	}

	/* socket ready for read/write */
	if (FD_ISSET(ipc->socket_fd, fds))
		return 0;

	/* socket not ready */
	return -EINVAL;
}

static int plug_ipc_cmd_tx(struct plug_socket_desc *ipc, void *msg, size_t len)
{
	fd_set write_fds;
	char mailbox[IPC3_MAX_MSG_SIZE];
	ssize_t bytes;
	int err;

	if (len > IPC3_MAX_MSG_SIZE) {
		SNDERR("ipc: message too big %d\n", len);
		return -EINVAL;
	}
	memset(mailbox, 0, IPC3_MAX_MSG_SIZE);
	memcpy(mailbox, msg, len);

	/* Wait for the socket to be writable */
	FD_ZERO(&write_fds);
	FD_SET(ipc->socket_fd, &write_fds);

	err = plug_socket_timed_wait(ipc, &write_fds, 20, true);
	if (err < 0)
		return err;

	bytes = send(ipc->socket_fd, mailbox, IPC3_MAX_MSG_SIZE, 0);
	if (bytes == -1) {
		SNDERR("failed to send IPC message : %s\n", strerror(errno));
		return -errno;
	}

	return bytes;
}

static int plug_ipc_cmd_rx(struct plug_socket_desc *ipc, char mailbox[IPC3_MAX_MSG_SIZE])
{
	fd_set read_fds;
	int err;

	/* Wait for the socket to be readable */
	FD_ZERO(&read_fds);
	FD_SET(ipc->socket_fd, &read_fds);

	err = plug_socket_timed_wait(ipc, &read_fds, 200, false);
	if (err < 0)
		return err;

	memset(mailbox, 0, IPC3_MAX_MSG_SIZE);
	return recv(ipc->socket_fd, mailbox, IPC3_MAX_MSG_SIZE, 0);
}

int plug_ipc_cmd_tx_rx(struct plug_socket_desc *ipc, void *msg, size_t len, void *reply,
		       size_t rlen)
{
	char mailbox[IPC3_MAX_MSG_SIZE];
	ssize_t bytes;
	int err;

	/* send IPC message */
	bytes = plug_ipc_cmd_tx(ipc, msg, len);
	if (bytes == -1) {
		SNDERR("failed to send IPC message : %s\n", strerror(errno));
		return -errno;
	}

	/* wait for response */
	memset(mailbox, 0, IPC3_MAX_MSG_SIZE);
	bytes = plug_ipc_cmd_rx(ipc, mailbox);
	if (bytes == -1) {
		SNDERR("failed to read IPC message reply %s\n", strerror(errno));
		return -errno;
	}

	/* no response or connection lost, try to restablish connection */
	if (bytes == 0) {
		close(ipc->socket_fd);
		err = plug_create_client_socket(ipc);
		if (err < 0) {
			SNDERR("failed to reestablish connection to SOF pipe IPC socket : %s",
			       strerror(err));
			return -errno;
		}

		/* send IPC message again */
		bytes = plug_ipc_cmd_tx(ipc, msg, len);
		if (bytes == -1) {
			SNDERR("failed to send IPC message : %s\n", strerror(errno));
			return -errno;
		}

		/* wait for response */
		memset(mailbox, 0, IPC3_MAX_MSG_SIZE);
		bytes = plug_ipc_cmd_rx(ipc, mailbox);
		if (bytes == -1) {
			SNDERR("failed to read IPC message reply %s\n", strerror(errno));
			return -errno;
		}

		/* connection lost again, quit now */
		if (bytes == 0)
			return -errno;
	}

	/* do the message work */
	if (rlen && reply)
		memcpy(reply, mailbox, rlen);

	return 0;
}

void plug_ctl_ipc_message(struct ipc4_module_large_config *config, int param_id,
			  size_t size, uint32_t module_id, uint32_t instance_id,
			  uint32_t type)
{
	config->primary.r.type = type;
	config->primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_MODULE_MSG;
	config->primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	config->primary.r.module_id = module_id;
	config->primary.r.instance_id = instance_id;

	config->extension.r.data_off_size = size;
	config->extension.r.large_param_id = param_id;
}

int plug_send_bytes_data(struct plug_socket_desc *ipc, uint32_t module_id, uint32_t instance_id,
			 struct sof_abi_hdr *abi)
{
	struct ipc4_module_large_config config = {{ 0 }};
	struct ipc4_message_reply reply;
	void *msg;
	int msg_size;
	int err;

	/* configure the IPC message */
	plug_ctl_ipc_message(&config, abi->type, abi->size, module_id, instance_id,
			     SOF_IPC4_MOD_LARGE_CONFIG_SET);

	config.extension.r.final_block = 1;
	config.extension.r.init_block = 1;

	/* allocate memory for IPC message */
	msg_size = sizeof(config) + abi->size;
	msg = calloc(msg_size, 1);
	if (!msg)
		return -ENOMEM;

	/* set the IPC message data */
	memcpy(msg, &config, sizeof(config));
	memcpy(msg + sizeof(config), abi->data, abi->size);

	/* send the message and check status */
	err = plug_ipc_cmd_tx_rx(ipc, msg, msg_size, &reply, sizeof(reply));
	free(msg);
	if (err < 0) {
		SNDERR("failed to send IPC to set bytes data\n");
		return err;
	}

	if (reply.primary.r.status != IPC4_SUCCESS) {
		SNDERR("IPC failed with status %d\n", reply.primary.r.status);
		return -EINVAL;
	}

	return 0;
}

int plug_socket_create(struct plug_socket_desc *ipc)
{
	struct sockaddr_un addr;
	int sockfd;

	/* Check if the socket path already exists */
	if (access(ipc->path, F_OK) != -1) {
		/* If it exists, remove it */
		if (unlink(ipc->path) == -1) {
			SNDERR("unlink previous socket file");
			return -EINVAL;
		}
	}

	/* Create the socket */
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd == -1) {
		SNDERR("failed to create new socket");
		return sockfd;
	}

	ipc->socket_fd = sockfd;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, ipc->path, sizeof(addr.sun_path) - 1);

	/* Bind the socket to the address */
	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		SNDERR("failed to bind new socket for IPC path\n");
		close(sockfd);
		return -EINVAL;
	}

	if (listen(sockfd, MAX_IPC_CLIENTS) == -1) {
		SNDERR("failed to listen on socket for IPC\n");
		return -EINVAL;
	}

	return 0;
}

static int set_socket_nonblocking(int sockfd)
{
	int flags = fcntl(sockfd, F_GETFL, 0);

	if (flags == -1) {
		SNDERR("fcntl(F_GETFL) failed");
		return -EINVAL;
	}

	flags |= O_NONBLOCK;
	if (fcntl(sockfd, F_SETFL, flags) == -1) {
		SNDERR("fcntl(F_SETFL) failed");
		return -EINVAL;
	}

	return 0;
}

int plug_create_client_socket(struct plug_socket_desc *ipc)
{
	struct sockaddr_un addr;
	int sockfd;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd == -1) {
		SNDERR("error: failed to create sof-pipe IPC socket\n");
		return sockfd;
	}

	if (set_socket_nonblocking(sockfd) < 0) {
		close(sockfd);
		return -EINVAL;
	}
	ipc->socket_fd = sockfd;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, ipc->path, sizeof(addr.sun_path) - 1);

	/* Connect to the server (non-blocking) */
	if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		if (errno != EINPROGRESS) {
			SNDERR("failed to connect to ipc socket");
			return -errno;
		}
	}

	return sockfd;
}
