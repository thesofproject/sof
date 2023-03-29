#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define __packed                        __attribute__((__packed__))
#define SOF_EXT_MAN_MAGIC_NUMBER	0x6e614d58
#define	EINVAL		22	/* Invalid argument */
#define SND_SOF_FW_SIG_SIZE	4
#define SND_SOF_FW_ABI		1
#define SND_SOF_FW_SIG		"Reef"

/* Build u32 number in format MMmmmppp */
#define SOF_EXT_MAN_BUILD_VERSION(MAJOR, MINOR, PATH) ((uint32_t)( \
	((MAJOR) << 24) | \
	((MINOR) << 12) | \
	(PATH)))

/* used extended manifest header version */
#define SOF_EXT_MAN_VERSION		SOF_EXT_MAN_BUILD_VERSION(1, 0, 0)

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned int __u32;

/*
 * Firmware file is made up of 1 .. N different modules types. The module
 * type is used to determine how to load and parse the module.
 */
enum sof_fw_mod_type {
	SOF_FW_BASE	= 0,	/* base firmware image */
	SOF_FW_MODULE	= 1,	/* firmware module */
};

/*
 * Firmware module is made up of 1 . N blocks of different types. The
 * Block header is used to determine where and how block is to be copied in the
 * DSP/host memory space.
 */
enum sof_fw_blk_type {
	SOF_FW_BLK_TYPE_INVALID	= -1,
	SOF_FW_BLK_TYPE_START	= 0,
	SOF_FW_BLK_TYPE_RSRVD0	= SOF_FW_BLK_TYPE_START,
	SOF_FW_BLK_TYPE_IRAM	= 1,	/* local instruction RAM */
	SOF_FW_BLK_TYPE_DRAM	= 2,	/* local data RAM */
	SOF_FW_BLK_TYPE_SRAM	= 3	/* system RAM */
};

struct firmware {
	size_t size;
	const u8 *data;

	/* firmware loader private fields */
	void *priv;
};

/* extended manifest header, deleting any field breaks backward compatibility */
struct ext_man_header {
	uint32_t magic;		/*< identification number, */
				/*< EXT_MAN_MAGIC_NUMBER */
	uint32_t full_size;	/*< [bytes] full size of ext_man, */
				/*< (header + content + padding) */
	uint32_t header_size;	/*< [bytes] makes header extensionable, */
				/*< after append new field to ext_man header */
				/*< then backward compatible won't be lost */
	uint32_t header_version; /*< value of EXT_MAN_VERSION */
				/*< not related with following content */

	/* just after this header should be list of ext_man_elem_* elements */
} __packed;

/* extended manifest element header */
struct sof_ext_man_elem_header {
	uint32_t type;		/*< SOF_EXT_MAN_ELEM_ */
	uint32_t size;		/*< in bytes, including header size */

	/* just after this header should be type dependent content */
} __packed;

struct sof_mod_hdr {
	enum sof_fw_mod_type type;
	__u32 size;		/* bytes minus this header */
	__u32 num_blocks;	/* number of blocks */
} __packed;

struct sof_fw_header {
	unsigned char sig[SND_SOF_FW_SIG_SIZE]; /* "Reef" */
	__u32 file_size;	/* size of file minus this header */
	__u32 num_modules;	/* number of modules */
	__u32 abi;		/* version of header format */
} __packed;

struct sof_blk_hdr {
	enum sof_fw_blk_type type;
	__u32 size;		/* bytes minus this header */
	__u32 offset;		/* offset from base */
} __packed;

static ssize_t ipc3_fw_ext_man_size(const struct firmware *fw)
{
	const struct ext_man_header *head;

	head = (struct ext_man_header *)fw->data;
	/*
	 * assert fw size is big enough to contain extended manifest header,
	 * it prevents from reading unallocated memory from `head` in following
	 * step.
	 */
	if (fw->size < sizeof(*head))
		return -EINVAL;

	/*
	 * When fw points to extended manifest,
	 * then first u32 must be equal SOF_EXT_MAN_MAGIC_NUMBER.
	 */
	if (head->magic == SOF_EXT_MAN_MAGIC_NUMBER)
		return head->full_size;
	/* otherwise given fw don't have an extended manifest */
	printf("dev_dbg : Unexpected extended manifest magic number: %#x\n",
	       head->magic);
	return 0;
}

static size_t sof_ipc3_fw_parse_ext_man(char *input_file_ptr, long file_size)
{
	struct firmware fw;
	const struct sof_ext_man_elem_header *elem_hdr;
	const struct ext_man_header *head;
	ssize_t ext_man_size;
	ssize_t remaining;
	uintptr_t iptr;
	int ret = 0;

	fw.size = (size_t)file_size;
	fw.data = (u8 *)input_file_ptr;
	head = (struct ext_man_header *)fw.data;
	remaining = head->full_size - head->header_size;
	printf("dev_dbg : head fullsize: %d  head headersize : %d\n", head->full_size,
	       head->header_size);

	ext_man_size = ipc3_fw_ext_man_size(&fw);
	printf("dev_dbg : ext_man_size is %ld\n", ext_man_size);

	/* Assert firmware starts with extended manifest */
	if (ext_man_size <= 0)
		return ext_man_size;

	/* get first extended manifest element header */
	iptr = (uintptr_t)fw.data + head->header_size;

	while (remaining > sizeof(*elem_hdr)) {
		elem_hdr = (struct sof_ext_man_elem_header *)iptr;
		//printf("dev_dbg : found sof_ext_man header type %d size %#x\n",
		//	elem_hdr->type, elem_hdr->size);

		if (elem_hdr->size < sizeof(*elem_hdr) ||
		    elem_hdr->size > remaining) {
			printf("dev_err : invalid sof_ext_man header size, type %d size %#x\n",
			       elem_hdr->type, elem_hdr->size);
			return -EINVAL;
		}

		remaining -= elem_hdr->size;
		iptr += elem_hdr->size;
	}

	if (remaining) {
		printf("dev_err : error: sof_ext_man header is inconsistent\n");
		return -EINVAL;
	}
	return ext_man_size;
}

//sof_dsp_block_write <==> acp_dsp_block_write

int sof_dsp_block_write(enum sof_fw_blk_type blk_type,
			u32 offset, void *src, size_t size, FILE *out_fptr)
{
	static int i;
	unsigned int size_fw;
	int ret;

	switch (blk_type) {
	case SOF_FW_BLK_TYPE_IRAM:
			break;
	case SOF_FW_BLK_TYPE_DRAM:
			break;
	case SOF_FW_BLK_TYPE_SRAM:
		{
			printf("%d == SOF_FW_BLK_TYPE_SRAM\n", i);
		}
	default:
		{}
	}
	ret = fwrite(src, size, 1, out_fptr);
	i++;
	return 0;
}

/* generic module parser for mmaped DSPs */
static int sof_ipc3_parse_module_memcpy(/*struct snd_sof_dev *sdev,*/
					struct sof_mod_hdr *module)
{
	struct sof_blk_hdr *block;
	int count, ret, i;
	u32 offset;
	size_t remaining;

	//printf("dev_dbg : new module size %#x blocks %#x type %#x\n",
	//	module->size, module->num_blocks, module->type);
	block = (struct sof_blk_hdr *)((u8 *)module + sizeof(*module));
	/* module->size doesn't include header size */
	remaining = module->size;

	//outfile opening logic
	FILE *out_image_fptr = fopen("fwimage_3_0.bin", "wb+");
	FILE *out_data_fptr = fopen("fwdata_3_0.bin", "wb+");
	__u32 prev_size = 0;
	u32 prev_offset = 0;
	enum sof_fw_blk_type prev_type;
	char one_byte = strtol("00000000", NULL, 2);

	for (count = 0; count < module->num_blocks; count++) {
		/* check for wrap */
		if (remaining < sizeof(*block)) {
			printf("dev_err : not enough data remaining\n");
			return -EINVAL;
		}

		/* minus header size of block */
		remaining -= sizeof(*block);

		if (block->size == 0) {
			printf("dev_warn : warning: block %d size zero\n", count);
			printf("dev_warn : type %#x offset %#x\n", block->type, block->offset);
			continue;
		}

		switch (block->type) {
		case SOF_FW_BLK_TYPE_RSRVD0:
			continue;	/* not handled atm */
		case SOF_FW_BLK_TYPE_IRAM:
		case SOF_FW_BLK_TYPE_DRAM:
		case SOF_FW_BLK_TYPE_SRAM:
			offset = block->offset;
			break;
		default:
			printf("dev_err : bad type %#x for block %#x\n", block->type, count);
			return -EINVAL;
		}

		printf("dev_dbg : block %d type %#x size %#x ==>  offset %#x\n",
		       count, block->type, block->size, offset);
		prev_size = block->size;
		prev_offset = offset;
		prev_type = block->type;

		/* checking block->size to avoid unaligned access */
		if (block->size % sizeof(u32)) {
			printf("dev_err : invalid block size %#x\n", block->size);
			return -EINVAL;
		}
		switch (block->type) {
		case SOF_FW_BLK_TYPE_IRAM:
			ret = sof_dsp_block_write(block->type, offset,
						  block + 1, block->size, out_image_fptr);
			break;
		case SOF_FW_BLK_TYPE_DRAM:
			ret = sof_dsp_block_write(block->type, offset,
						  block + 1, block->size, out_data_fptr);
			break;
		default:
			printf("dev_err : Invalid block type\n");
		}
		if (ret < 0) {
			printf("dev_err : write to block type %#x failed\n", block->type);
			return ret;
		}

		if (remaining < block->size) {
			printf("dev_err : not enough data remaining\n");
			return -EINVAL;
		}

		/* minus body size of block */
		remaining -= block->size;

		/* next block */
		block = (struct sof_blk_hdr *)((u8 *)block + sizeof(*block)
			+ block->size);

		if (!(block->offset == 0)) {
			switch (prev_type) {
			case SOF_FW_BLK_TYPE_IRAM:
				for (i = 0; i < ((block->offset - prev_offset) - prev_size); i++)
					fwrite(&one_byte, 1, 1, out_image_fptr);
				break;
			case SOF_FW_BLK_TYPE_DRAM:
				for (i = 0; i < ((block->offset - prev_offset) - prev_size); i++)
					fwrite(&one_byte, 1, 1, out_data_fptr);
				break;
			default:
				printf("writing nothing extra to output file\n");
			}
		}
	}
	fclose(out_data_fptr);
	fclose(out_image_fptr);
	return 0;
}

static int sof_ipc3_load_fw_to_dsp(u32 inp_payload_offset, char *input_file_ptr, long file_size)
{
	u32 payload_offset = inp_payload_offset;
	struct firmware fw;

	fw.size = (size_t)file_size;
	fw.data = (u8 *)input_file_ptr;
	struct sof_fw_header *header;
	struct sof_mod_hdr *module;
	size_t remaining;
	int ret, count;

	header = (struct sof_fw_header *)(fw.data + payload_offset);
	printf("dev_dbg : Using generic module loading\n");

	/* parse each module */
	module = (struct sof_mod_hdr *)(fw.data + payload_offset + sizeof(*header));

	remaining = fw.size - sizeof(*header) - payload_offset;
	/* check for wrap */
	if (remaining > fw.size) {
		printf("dev_err : fw size smaller than header size\n");
		return -EINVAL;
	}

	// printf("header->num_modules : %d\n",header->num_modules);

	for (count = 0; count < header->num_modules; count++) {
		/* check for wrap */
		if (remaining < sizeof(*module)) {
			printf("dev_err : not enough data for a module\n");
			return -EINVAL;
		}

		/* minus header size of module */
		remaining -= sizeof(*module);

		/* module */
		ret = sof_ipc3_parse_module_memcpy(module);
		if (ret < 0) {
			printf("dev_err : invalid module\n");
			return ret;
		}

		if (remaining < module->size) {
			printf("dev_err : not enough data remaining\n");
			return -EINVAL;
		}

		/* minus body size of module */
		remaining -=  module->size;
		module = (struct sof_mod_hdr *)((u8 *)module +
			 sizeof(*module) + module->size);
	}
	return 0;
}

int main(void)
{
	int ret;
	u32 payload_offset;
	FILE *inpt_fw_fptr = fopen("sof-vangogh.ri", "rb+");

	// To find the length of input ri image file
	fseek(inpt_fw_fptr, 0, SEEK_END);
	long fsize = ftell(inpt_fw_fptr);

	fseek(inpt_fw_fptr, 0, SEEK_SET);
	char *inp_buffer = malloc(fsize + 1);

	fread(inp_buffer, fsize, 1, inpt_fw_fptr);
	printf("dev_dbg : Read a file of size %ld bytes\n", fsize);

	int ext_man_size = sof_ipc3_fw_parse_ext_man(inp_buffer, fsize);
	// printf("extended manifesto size is %d bytes\n", ext_man_size);

	if (ext_man_size > 0) {
		/* when no error occurred, drop extended manifest */
	// payload_offset <==> sdev->basefw.payload_offset
		payload_offset = ext_man_size;
	} else if (!ext_man_size) {
		/* No extended manifest, so nothing to skip during FW load */
		printf("dev_err : firmware doesn't contain extended manifest\n");
		return 0;
	} else {
		ret = ext_man_size;
		printf("dev_err : error: firmware sof-vangogh.ri contains unsupported or \
		      invalid extended manifest: %d\n", ret);
	}
	ret = sof_ipc3_load_fw_to_dsp(payload_offset, inp_buffer, fsize);

	fclose(inpt_fw_fptr);
	free(inp_buffer);
	return 0;
}
