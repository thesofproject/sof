/* SRC conversions */
#include <reef/audio/coefficients/src/src_std_int24_1_2_2188_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_1_2_4375_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_1_3_2188_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_1_3_4375_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_2_1_2188_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_2_1_4375_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_2_3_4375_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_3_1_2188_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_3_1_4375_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_3_2_4375_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_3_4_4375_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_4_3_4375_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_5_7_4375_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_7_8_4375_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_8_7_2381_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_8_7_4375_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_8_21_3125_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_10_9_4375_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_10_21_4375_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_16_7_3938_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_20_7_2871_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_20_21_4020_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_21_20_4020_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_21_40_3828_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_21_80_3828_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_32_21_4375_5000.h>
#include <reef/audio/coefficients/src/src_std_int24_40_21_3828_5000.h>

/* SRC table */
int32_t fir_one = 4194304;
struct src_stage src_int24_1_1_0_0 =  { 0, 0, 1, 1, 1, 1, 1, 0, -1, &fir_one };
struct src_stage src_int24_0_0_0_0 =  { 0, 0, 0, 0, 0, 0, 0, 0,  0, &fir_one };
int src_in_fs[15] = { 8000, 11025, 12000, 16000, 18900, 22050, 24000, 32000,
	 44100, 48000, 64000, 88200, 96000, 176400, 192000};
int src_out_fs[10] = { 8000, 11025, 12000, 16000, 18900, 22050, 24000, 32000,
	 44100, 48000};
struct src_stage *src_table1[10][15] = {
	{ &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_1_2_4375_5000,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_3_4375_5000, &src_int24_1_2_2188_5000,
	 &src_int24_0_0_0_0, &src_int24_1_3_2188_5000,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0},
	{ &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_21_80_3828_5000, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0
	},
	{ &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_1_2_2188_5000,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0},
	{ &src_int24_2_1_4375_5000,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_2_3_4375_5000,
	 &src_int24_1_2_4375_5000, &src_int24_0_0_0_0,
	 &src_int24_1_3_4375_5000, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0
	},
	{ &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0},
	{ &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_21_40_3828_5000, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0
	},
	{ &src_int24_3_1_4375_5000, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_3_2_4375_5000,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_3_4_4375_5000,
	 &src_int24_0_0_0_0, &src_int24_1_2_4375_5000,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0},
	{ &src_int24_2_1_4375_5000,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_2_1_4375_5000, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_4_3_4375_5000,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_2_3_4375_5000, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0
	},
	{ &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_21_20_4020_5000,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0},
	{ &src_int24_2_1_4375_5000,
	 &src_int24_32_21_4375_5000, &src_int24_2_1_4375_5000,
	 &src_int24_3_1_4375_5000, &src_int24_10_9_4375_5000,
	 &src_int24_8_7_4375_5000, &src_int24_2_1_4375_5000,
	 &src_int24_3_2_4375_5000, &src_int24_8_7_4375_5000,
	 &src_int24_1_1_0_0, &src_int24_3_4_4375_5000,
	 &src_int24_8_7_2381_5000, &src_int24_1_2_4375_5000,
	 &src_int24_8_21_3125_5000, &src_int24_1_2_2188_5000
	}
};
struct src_stage *src_table2[10][15] = {
	{ &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_1_1_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_1_2_4375_5000,
	 &src_int24_0_0_0_0, &src_int24_1_2_4375_5000,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0},
	{ &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_7_8_4375_5000, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0
	},
	{ &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_1_2_4375_5000,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0},
	{ &src_int24_1_1_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_1_1_0_0,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0
	},
	{ &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0},
	{ &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_7_8_4375_5000, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0
	},
	{ &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_1_1_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_1_1_0_0,
	 &src_int24_0_0_0_0, &src_int24_1_1_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0},
	{ &src_int24_2_1_2188_5000,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_1_1_0_0,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0
	},
	{ &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_1_1_0_0, &src_int24_7_8_4375_5000,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0, &src_int24_0_0_0_0,
	 &src_int24_0_0_0_0},
	{ &src_int24_3_1_2188_5000,
	 &src_int24_20_7_2871_5000, &src_int24_2_1_2188_5000,
	 &src_int24_1_1_0_0, &src_int24_16_7_3938_5000,
	 &src_int24_40_21_3828_5000, &src_int24_1_1_0_0,
	 &src_int24_1_1_0_0, &src_int24_20_21_4020_5000,
	 &src_int24_1_1_0_0, &src_int24_1_1_0_0,
	 &src_int24_10_21_4375_5000, &src_int24_1_1_0_0,
	 &src_int24_5_7_4375_5000, &src_int24_1_2_4375_5000
	}
};
