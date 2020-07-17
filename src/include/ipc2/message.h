/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 */

#ifndef SRC_INCLUDE_IPC2_MESSAGE_H_
#define SRC_INCLUDE_IPC2_MESSAGE_H_

#include <stdint.h>

/*
 * High Level
 * ==========
 *
 * IPC2 messaging works at a high level on the basic principle of a standard
 * header followed by either a tuple array of data OR a private data structure.
 *
 * Tuple Mode
 * ----------
 *
 * +------------------------+
 * | IPC2 header            |
 * |   route (optional)     |
 * |   size (mandatory)     |
 * |   elems (mandatory)    |
 * +------------------------+
 * | Tuple ID | Data        |
 * +------------------------+
 * | Tuple ID | Data        |
 * +------------------------+
 * | Tuple ID | Data        |
 * +------------------------+
 * | etc ...................|
 * +------------------------+
 *
 * (example 1 - IPC2 header with tuples)
 *
 * The tuple data is either fixed size or variable size and the tuples
 * are unordered in the IPC message.
 *
 * Tuple mode is enabled by setting hdr.size = 1 and hdr.elems = 1
 *
 * Tuple data can be represented by creating arrays using any combinations of
 * struct ipc2_elem_std, ipc2_elem_micro and ipc2_elem_micro_array. This
 * provides flexibility over data expression and data density.
 *
 *
 * Private Data Mode
 * -----------------
 *
 * +------------------------+
 * | IPC2 header            |
 * |   route (optional)     |
 * |   size (optional)      |
 * |   elems (optional)     |
 * +------------------------+
 * | Private data block     |
 * +------------------------+
 *
 * (example 2 - IPC2 header with private data)
 *
 * The private data can be anything - The primary use case is C data structures
 * from previous IPC ABIs. Private data only mode does not use the tuples below
 * but uses existing or legacy IPC ABI structures.
 *
 * Private data block only mode is enabled by setting hdr.block = 1.
 *
 *
 * Tuples
 * ======
 *
 * Tuples come in two types where type is determined by MSB of tuple ID. This
 * is to provide flexibility for message density and data size. i.e. small
 * tuple dense messages are supported alongside large messages with variable
 * tuple density.
 *
 * 1) Standard tuple element - Minimum 2 words (1 data word) - max 256kB
 *
 * 2) Micro tuple element - Fixed size 1 word (1 data short).
 *
 * Where the tuple IDs are in a continuous range then the protocol can compress
 * tuples and omit IDs for each tuple are ID[0] (the base tuple ID) meaning the
 * tuple array is data only with each subsequent word/short being the next
 * element in the array.
 *
 * Tuple IDs
 * ---------
 *
 * The tuple ID is a 15bit number unique only to the class, subclass, AND action
 * meaning each action can have up to 2^14 standard tuple data elements and 2^14
 * micro tuple elements or 2^15 data element IDs in total.
 *
 * This tuple ID range gives enough head room for ID deprication and new ID
 * additions without having to add and code new actions.
 */


/*
 * IPC2.0 Tuple ID.
 *
 * The tuple ID has a 15 bit ID value and a 1 bit type flag indication whether
 * it uses struct ipc2_std_elem or struct ipc2_micro_elem for data below.
 *
 * The array flag indicates that the protocol is sending a continuous sequence
 * of tuple IDs and has compressed the data.
 */

struct ipc2_tuple {
	uint16_t array : 1;	/* tuple is an array of tuples NOT part of ID */
	uint16_t hd : 1;	/* tuple ID - high density "micro elem" */
	uint16_t id : 14;	/* tuple ID - specific to class/subclass/action */
} __attribute__((packed));

/*
 * IPC2.0 - generic tuple data element.
 *
 * Generic tuple type that can be used for either a single tuple
 * (tuple.array = 0) or for an array of tuples (tuple.array = 1).
 *
 * Single tuple mode can represent from 4 bytes to 256kB of data.
 * Array mode can represent 2^16 tuples of size 4 bytes.
 *
 * Tuple RAW IDs - 0x0000 ... 0x7FFF (as tuple.hd is set to 0). Array uses
 * tuple ID as base array index.
 */
struct ipc2_elem_std {
	struct ipc2_tuple tuple;	/* tuple ID and type */
	union {
		uint16_t size;		/* data size in words  - max 256kB */
		uint16_t count;		/* number of data items in tuple array */
	};
	uint32_t data[];		/* tuple data */
} __attribute__((packed));

/*
 * IPC2.0 - Micro tuple element.
 *
 * Micro tuple type that can be used for 2 bytes of data.
 * Tuple RAW IDs - 0x8000 ... 0xFFFF (as tuple.hd = 1)
 */
struct ipc2_elem_micro {
	struct ipc2_tuple tuple;	/* tuple ID and type */
	uint16_t data;			/* tuple data */
} __attribute__((packed));

/*
 * IPC2.0 - Micro tuple element array.
 *
 * Micro tuple array type that can be used for array of 2 byte data.
 * Tuple RAW IDs - 0x8000 ... 0xFFFF (as tuple.hd = 1 and tuple.array = 1)
 */
struct ipc2_elem_micro_array {
	struct ipc2_tuple tuple;	/* tuple ID and type */
	uint16_t count;			/* tuple array count */
	uint16_t data[];		/* tuple data */
} __attribute__((packed));

/*
 * Tuple elem convenience macros
 */

/* size of micro tuple elem */
/* get size of standard tuple and data in bytes */
#define SOF_IPC_ELEM_MICRO_SIZE(elem) \
	(elem->tuple.array ?		\
		sizeof(ipc2_elem_micro) + elem->count * sizeof(uint16_t) :\
		sizeof(ipc2_elem_micro)

/* get size of standard tuple and data in bytes */
#define SOF_IPC_ELEM_STD_SIZE(elem) \
	(sizeof(ipc2_elem_std) + elem->size * sizeof(uint32))

#endif /* SRC_INCLUDE_IPC2_MESSAGE_H_ */
