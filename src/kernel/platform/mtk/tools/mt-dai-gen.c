// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2024 Google LLC.  All rights reserved.
// Author: Andy Ross <andyross@google.com>
#include <sof/lib/dai-legacy.h>
#include <sof/drivers/afe-drv.h>
#include <stdio.h>

/* DIY assertion, an "assert()" is already defined in platform headers */
#define CHECK(expr) do { if (!(expr)) {				    \
		printf("FAILED: " #expr " at line %d\n", __LINE__); \
		*(int *)0 = 0; }				    \
	} while (false)

/* These are the symbols we need to enumerate */
extern struct mtk_base_afe_platform mtk_afe_platform;
extern const struct dai_info lib_dai;

/* Call this to initialize the dai arrays */
int dai_init(struct sof *sof);

/* Debug hook in some versions of MTK firmware */
void printf_(void) {}

/* Just need a pointer to a symbol with this name */
int afe_dai_driver;

/* So dai_init() can write to something */
struct sof sof;

unsigned int afe_base_addr;

void symify(char *s)
{
	for (; *s; s++) {
		if (*s >= 'A' && *s <= 'Z')
			*s += 'a' - 'A';
		CHECK((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '_');
	}
}

/* The AFE driver has some... idiosyncratic defaulting.  The existing
 * configurations have a varying set of conventions to encode "no
 * value is set":
 *
 * ch_num is skipped if the stored reg value is negative
 * quad_ch is skipped if the mask is zero
 * int_odd: reg <=0
 * mono: reg <=0 OR shift <=0
 * msb: reg <=0
 * msb2: reg <=0
 * agent_disable: reg <=0
 * fs: never skipped
 * hd: never skipped
 * enable: never skipped
 *
 * We detect the union of those conditions and elide the setting (it
 * will be defaulted to reg=-1/shift=0/mask=0 in the driver DTS macros)
 */
void print_fld(const char *name, int reg, int shift, int lomask)
{
	if (reg <= 0 || shift < 0 || lomask == 0)
		return;

	int bits = __builtin_ffs(lomask + 1) - 1;

	CHECK(((lomask + 1) & lomask) == 0);       /* must be proper mask in low bits */
	CHECK(lomask);                             /* and not zero */
	CHECK(shift >= 0 && (shift + bits) <= 32); /* and shift doesn't overrun */

	printf("\t\t%s = <0x%8.8x %d %d>;\n",
	       name, reg + afe_base_addr, shift, bits);
}

unsigned int msbaddr(int val)
{
	return val ? val + afe_base_addr : 0;
}

int main(void)
{
	dai_init(&sof);

	afe_base_addr = mtk_afe_platform.base_addr;

	/* The DAI order here is immutable: the indexes are known to and
	 * used by the kernel driver.  And these point to the memif array
	 * via an index stored in the low byte (?!) of the first fifo's
	 * "handshake" (it's not a DMA handshake value at all).  So we
	 * invert the mapping and store the dai index along with the AFE
	 * record.
	 */
	int dai_memif[64];
	int num_dais = 0;

	for (int t = 0; t < lib_dai.num_dai_types; t++) {
		for (int i = 0; i < lib_dai.dai_type_array[t].num_dais; i++) {
			int idx = lib_dai.dai_type_array[t].dai_array[i].index;
			int hs = lib_dai.dai_type_array[t].dai_array[i].plat_data.fifo[0].handshake;

			CHECK(idx == num_dais);
			dai_memif[num_dais++] = hs >> 16;
		}
	}

	/* Quick check that the dai/memif mapping is unique */
	for (int i = 0; i < num_dais; i++) {
		int n = 0;

		for (int j = 0; j < num_dais; j++)
			if (dai_memif[j] == i)
				n++;
		CHECK(n == 1);
	}

	for (int i = 0; i < mtk_afe_platform.memif_size; i++) {
		const struct mtk_base_memif_data *m = &mtk_afe_platform.memif_datas[i];

		int dai_id = -1;

		for (int j = 0; j < num_dais; j++) {
			if (dai_memif[j] == i) {
				dai_id = j;
				break;
			}
		}
		CHECK(dai_id >= 0);

		/* We use the UL/DL naming to detect direction, make
		 * sure it isn't broken
		 */
		bool uplink = !!strstr(m->name, "UL");
		bool downlink = !!strstr(m->name, "DL");

		CHECK(uplink != downlink);

		/* Validate and lower-case the name to make a DTS symbol */
		char sym[64];

		CHECK(strlen(m->name) < sizeof(sym));
		strcpy(sym, m->name);
		symify(sym);

		printf("\tafe_%s: afe_%s {\n", sym, sym);
		printf("\t\tcompatible = \"mediatek,afe\";\n");
		printf("\t\tafe_name = \"%s\";\n", m->name);
		printf("\t\tdai_id = <%d>;\n", dai_id);
		if (downlink)
			printf("\t\tdownlink;\n");

		/* Register pairs containing the high and low words of
		 * bus/host addresses.  The first (high) register is allowed
		 * to be zero indicating all addresses will be 32 bit.
		 */
		printf("\t\tbase = <0x%8.8x 0x%8.8x>;\n",
		       msbaddr(m->reg_ofs_base_msb), m->reg_ofs_base + afe_base_addr);
		printf("\t\tcur = <0x%8.8x 0x%8.8x>;\n",
		       msbaddr(m->reg_ofs_cur_msb), m->reg_ofs_cur + afe_base_addr);
		printf("\t\tend = <0x%8.8x 0x%8.8x>;\n",
		       msbaddr(m->reg_ofs_end_msb), m->reg_ofs_end + afe_base_addr);

		print_fld("fs", m->fs_reg, m->fs_shift, m->fs_maskbit);
		print_fld("mono", m->mono_reg, m->mono_shift, 1);
		if (m->mono_invert)
			printf("\t\tmono_invert;\n");
		print_fld("quad_ch", m->quad_ch_reg, m->quad_ch_shift, m->quad_ch_mask);
		print_fld("int_odd", m->int_odd_flag_reg, m->int_odd_flag_shift, 1);
		print_fld("enable", m->enable_reg, m->enable_shift, 1);
		print_fld("hd", m->hd_reg, m->hd_shift, 1);
		print_fld("msb", m->msb_reg, m->msb_shift, 1);
		print_fld("msb2", m->msb2_reg, m->msb2_shift, 1);
		print_fld("agent_disable", m->agent_disable_reg, m->agent_disable_shift, 1);
		print_fld("ch_num", m->ch_num_reg, m->ch_num_shift, m->ch_num_maskbit);

		/* Note: there are also "pbuf" and "minlen" registers defined
		 * in the memif_data struct, but they are unused by the
		 * existing driver.
		 */

		printf("\t};\n\n");
	}
}
