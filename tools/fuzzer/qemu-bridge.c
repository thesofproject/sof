// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

/*
 * Creates an IO bridge between two QEMU instances where messages can be passed
 * between the parent and child instances via messages queues and shared memory.
 *
 * The parent is usually the QEMU instance that runs the operating system (like
 * Linux) on the application processor whilst the child is typically a smaller
 * processor running an embedded firmware. The parent and child do not need to
 * be the same architecture but are expected to communicate over a local bus.
 */

#include <mqueue.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <glib.h>
#include <errno.h>
#include "qemu-bridge.h"

/* set to 1 to enable debug */
static int io_bridge_debug;//QEMU_IO_DEBUG;

/* we can either be parent or child */
#define ROLE_NONE    0
#define ROLE_PARENT    1
#define ROLE_CHILD    2

static int role = ROLE_NONE;

#define QEMU_IO_MAX_MSGS    8
#define QEMU_IO_MAX_MSG_SIZE    128
#define QEMU_IO_MAX_SHM_REGIONS    32

#define NAME_SIZE       64

struct io_shm {
	int fd;
	void *addr;
	char name[NAME_SIZE];
	size_t size;
};

struct io_mq {
	char mq_name[NAME_SIZE];
	char thread_name[NAME_SIZE];
	struct mq_attr mqattr;
	mqd_t mqdes;
};

struct io_bridge {
	struct io_mq parent;
	struct io_mq child;
	GThread *io_thread;
	int (*cb)(void *data, struct qemu_io_msg *msg);
	struct io_shm shm[QEMU_IO_MAX_SHM_REGIONS];
	void *data;
};

static struct io_bridge _iob;
static int _id;

/* parent reader Q */
static gpointer parent_reader_thread(gpointer data)
{
	struct io_bridge *io = data;
	char buf[QEMU_IO_MAX_MSG_SIZE];

	mq_getattr(io->parent.mqdes, &io->parent.mqattr);
	if (io_bridge_debug)
		fprintf(stdout, "bridge-io: %d messages are currently on parent queue.\n",
			(int)io->parent.mqattr.mq_curmsgs);

	while (mq_receive(io->parent.mqdes, buf, QEMU_IO_MAX_MSG_SIZE,
			  NULL) != -1) {
		struct qemu_io_msg *hdr = (struct qemu_io_msg *)buf;

		if (io_bridge_debug)
			fprintf(stdout, "bridge-io: msg recv %d type %d size %d msg %d\n",
				hdr->id, hdr->type, hdr->size, hdr->msg);

		if (io->cb)
			io->cb(io->data, hdr);
	}

	return 0;
}

/* child reader Q */
static gpointer child_reader_thread(gpointer data)
{
	struct io_bridge *io = data;
	char buf[QEMU_IO_MAX_MSG_SIZE];
	int i;

	mq_getattr(io->child.mqdes, &io->child.mqattr);
	if (io_bridge_debug)
		fprintf(stdout, "bridge-io: %d messages are currently on child queue.\n",
			(int)io->child.mqattr.mq_curmsgs);

	/* flush old messages here */
	for (i = 0; i < io->child.mqattr.mq_curmsgs; i++) {
		mq_receive(io->child.mqdes, buf, QEMU_IO_MAX_MSG_SIZE, NULL);
		struct qemu_io_msg *hdr = (struct qemu_io_msg *)buf;

		if (io_bridge_debug)
			fprintf(stdout, "bridge-io: flushed %d type %d size %d msg %d\n",
				hdr->id, hdr->type, hdr->size, hdr->msg);
	}

	while (mq_receive(io->child.mqdes, buf, QEMU_IO_MAX_MSG_SIZE,
			  NULL) != -1) {
		struct qemu_io_msg *hdr = (struct qemu_io_msg *)buf;

		if (io_bridge_debug)
			fprintf(stdout, "bridge-io: msg recv %d type %d size %d msg %d\n",
				hdr->id, hdr->type, hdr->size, hdr->msg);

		if (io->cb)
			io->cb(io->data, hdr);
	}

	return 0;
}

static int mq_init(const char *name, struct io_bridge *io)
{
	int ret = 0;

	io->parent.mqattr.mq_maxmsg = QEMU_IO_MAX_MSGS;
	io->parent.mqattr.mq_msgsize = QEMU_IO_MAX_MSG_SIZE;
	io->parent.mqattr.mq_flags = 0;
	io->parent.mqattr.mq_curmsgs = 0;

	io->child.mqattr.mq_maxmsg = QEMU_IO_MAX_MSGS;
	io->child.mqattr.mq_msgsize = QEMU_IO_MAX_MSG_SIZE;
	io->child.mqattr.mq_flags = 0;
	io->child.mqattr.mq_curmsgs = 0;

	if (role == ROLE_PARENT) {

		/* Host */

		sprintf(io->parent.thread_name, "io-bridge-%s", name);
		io->io_thread = g_thread_new(io->parent.thread_name,
					     parent_reader_thread, io);

		/* parent Rx Q */
		sprintf(io->parent.mq_name, "/qemu-io-parent-%s", name);
		io->parent.mqdes = mq_open(io->parent.mq_name,
					   O_RDONLY | O_CREAT,
					   0664, &io->parent.mqattr);
		if (io->parent.mqdes < 0) {
			fprintf(stderr, "failed to open parent Rx queue %d\n",
				-errno);
			ret = -errno;
		}

		/* parent Tx Q */
		sprintf(io->child.mq_name, "/qemu-io-child-%s", name);
		io->child.mqdes = mq_open(io->child.mq_name,
					  O_WRONLY | O_CREAT,
					  0664, &io->child.mqattr);
		if (io->child.mqdes < 0) {
			fprintf(stderr, "failed to open parent Tx queue %d\n",
				-errno);
			ret = -errno;
		}
	} else {

		/* DSP */

		sprintf(io->child.thread_name, "io-bridge-%s", name);
		io->io_thread = g_thread_new(io->child.thread_name,
					     child_reader_thread, io);

		/* child Rx Q */
		sprintf(io->child.mq_name, "/qemu-io-child-%s", name);
		mq_unlink(io->child.mq_name);
		io->child.mqdes = mq_open(io->child.mq_name,
					  O_RDONLY | O_CREAT,
					  0664, &io->child.mqattr);
		if (io->child.mqdes < 0) {
			fprintf(stderr, "failed to open child Rx queue %d\n",
				-errno);
			ret = -errno;
		}

		/* child Tx Q */
		sprintf(io->parent.mq_name, "/qemu-io-parent-%s", name);
		mq_unlink(io->parent.mq_name);
		io->parent.mqdes = mq_open(io->parent.mq_name,
					   O_WRONLY | O_CREAT,
					   0664, &io->parent.mqattr);
		if (io->parent.mqdes < 0) {
			fprintf(stderr, "failed to open child Tx queue %d\n",
				-errno);
			ret = -errno;
		}
	}

	if (ret == 0 && io_bridge_debug) {
		fprintf(stdout, "bridge-io-mq: added %s\n",
			io->parent.mq_name);
		fprintf(stdout, "bridge-io-mq: added %s\n", io->child.mq_name);
	}

	return ret;
}

int qemu_io_register_parent(const char *name,
			    int (*cb)(void *, struct qemu_io_msg *msg),
			    void *data)
{
	if (role != ROLE_NONE)
		return -EINVAL;

	role = ROLE_PARENT;
	_iob.cb = cb;
	_iob.data = data;

	mq_init(name, &_iob);

	return 0;
}

int qemu_io_register_child(const char *name,
			   int (*cb)(void *, struct qemu_io_msg *msg),
			   void *data)
{
	int ret = 0;

	if (role != ROLE_NONE)
		return -EINVAL;

	role = ROLE_CHILD;
	_iob.cb = cb;
	_iob.data = data;

	mq_init(name, &_iob);

	return ret;
}

int qemu_io_register_shm(const char *rname, int region, size_t size,
			 void **addr)
{
	char *name;
	int fd, ret;
	void *a;

	/* check that region is not already in use */
	if (_iob.shm[region].fd)
		return -EBUSY;

	name = _iob.shm[region].name;
	sprintf(name, "qemu-bridge-%s", rname);

	fd = shm_open(name, O_RDWR | O_CREAT, 0664);
	if (fd < 0) {
		fprintf(stderr, "bridge-io: can't open SHM %d\n", errno);
		return -errno;
	}

	ret = ftruncate(fd, size);
	if (ret < 0) {
		fprintf(stderr, "bridge-io: can't truncate %d\n", errno);
		shm_unlink(name);
		return -errno;
	}

	a = mmap(*addr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (!a) {
		fprintf(stderr, "bridge-io: can't open mmap %d\n", errno);
		shm_unlink(name);
		return -errno;
	}

	if (io_bridge_debug)
		fprintf(stdout, "bridge-io: %s fd %d region %d at %p allocated %zu bytes\n",
			name, fd, region, a, size);
	_iob.shm[region].fd = fd;
	_iob.shm[region].addr = a;
	_iob.shm[region].size = size;
	*addr = a;

	return ret;
}

#define PAGE_SIZE 4096

int qemu_io_sync(int region, unsigned int offset, size_t length)
{
	if (region < 0 || region > QEMU_IO_MAX_SHM_REGIONS)
		return -EINVAL;

	/* check that region is in use */
	if (_iob.shm[region].fd == 0)
		return -EINVAL;

	/* align offset to pagesize */
	offset -= (offset % PAGE_SIZE);

	return msync(_iob.shm[region].addr + offset, length,
		     MS_SYNC | MS_INVALIDATE);
}

int qemu_io_send_msg(struct qemu_io_msg *msg)
{
	int ret;

	msg->id = _id++;

	if (role == ROLE_PARENT)
		ret = mq_send(_iob.child.mqdes, (const char *)msg, msg->size,
			      0);
	else
		ret = mq_send(_iob.parent.mqdes, (const char *)msg, msg->size,
			      0);

	if (io_bridge_debug)
		fprintf(stdout, "bridge-io: msg send: %d type %d msg %d size %d ret %d\n",
			msg->id, msg->type, msg->msg, msg->size, ret);
	if (ret < 0)
		fprintf(stderr, "bridge-io: msg send failed %d\n", -errno);

	return ret;
}

int qemu_io_send_msg_reply(struct qemu_io_msg *msg)
{
	int ret;

	if (role == ROLE_PARENT)
		ret = mq_send(_iob.child.mqdes, (const char *)msg, msg->size,
			      0);
	else
		ret = mq_send(_iob.parent.mqdes, (const char *)msg, msg->size,
			      0);

	if (io_bridge_debug)
		fprintf(stdout, "bridge-io: repmsg send: %d type %d msg %d size %d ret %d\n",
			msg->id, msg->type, msg->msg, msg->size, ret);
	if (ret < 0)
		fprintf(stderr, "bridge-io: rmsg send failed %d\n", -errno);

	return ret;
}

void qemu_io_free(void)
{
	int i;

	for (i = 0; i < QEMU_IO_MAX_SHM_REGIONS; i++) {
		if (_iob.shm[i].fd) {
			munmap(_iob.shm[i].addr, _iob.shm[i].size);
			shm_unlink(_iob.shm[i].name);
			close(_iob.shm[i].fd);
		}
	}

	mq_unlink(_iob.parent.mq_name);
	mq_unlink(_iob.child.mq_name);

	mq_close(_iob.parent.mqdes);
	mq_close(_iob.child.mqdes);
}

void qemu_io_free_shm(int region)
{
	int err;

	if (region < QEMU_IO_MAX_SHM_REGIONS && _iob.shm[region].fd) {
		err = munmap(_iob.shm[region].addr, _iob.shm[region].size);
		if (err < 0)
			fprintf(stderr, "bridge-io: munmap failed %d\n",
				errno);

		/* client or host can unlink this, so it gets done twice */
		shm_unlink(_iob.shm[region].name);
		close(_iob.shm[region].fd);
		_iob.shm[region].fd = 0;
	}
}
