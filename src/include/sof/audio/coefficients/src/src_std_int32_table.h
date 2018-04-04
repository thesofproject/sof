/* SRC conversions */
#include <sof/audio/coefficients/src/src_std_int32_1_2_2292_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_1_2_4583_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_1_3_2292_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_1_3_4583_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_2_1_2292_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_2_1_4583_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_2_3_4583_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_3_1_2292_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_3_1_4583_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_3_2_4583_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_3_4_4583_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_4_3_4583_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_5_7_4583_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_7_8_4583_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_8_7_2494_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_8_7_4583_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_8_21_3274_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_10_9_4583_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_10_21_4583_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_16_7_4125_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_20_7_3008_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_20_21_4211_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_21_20_4211_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_21_40_4010_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_21_80_4010_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_32_21_4583_5000.h>
#include <sof/audio/coefficients/src/src_std_int32_40_21_4010_5000.h>

/* SRC table */
int32_t fir_one = 1073741824;
struct src_stage src_int32_1_1_0_0 =  { 0, 0, 1, 1, 1, 1, 1, 0, -1, &fir_one };
struct src_stage src_int32_0_0_0_0 =  { 0, 0, 0, 0, 0, 0, 0, 0,  0, &fir_one };
int src_in_fs[15] = { 8000, 11025, 12000, 16000, 18900, 22050, 24000, 32000,
	 44100, 48000, 64000, 88200, 96000, 176400, 192000};
int src_out_fs[10] = { 8000, 11025, 12000, 16000, 18900, 22050, 24000, 32000,
	 44100, 48000};
struct src_stage *src_table1[10][15] = {
	{ &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_2_4583_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_3_4583_5000, &src_int32_1_2_2292_5000,
	 &src_int32_0_0_0_0, &src_int32_1_3_2292_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_21_80_4010_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_2_2292_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_2_1_4583_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_2_3_4583_5000,
	 &src_int32_1_2_4583_5000, &src_int32_0_0_0_0,
	 &src_int32_1_3_4583_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_21_40_4010_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_3_1_4583_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_3_2_4583_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_3_4_4583_5000,
	 &src_int32_0_0_0_0, &src_int32_1_2_4583_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_2_1_4583_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_2_1_4583_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_4_3_4583_5000,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_2_3_4583_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_21_20_4211_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_2_1_4583_5000,
	 &src_int32_32_21_4583_5000, &src_int32_2_1_4583_5000,
	 &src_int32_3_1_4583_5000, &src_int32_10_9_4583_5000,
	 &src_int32_8_7_4583_5000, &src_int32_2_1_4583_5000,
	 &src_int32_3_2_4583_5000, &src_int32_8_7_4583_5000,
	 &src_int32_1_1_0_0, &src_int32_3_4_4583_5000,
	 &src_int32_8_7_2494_5000, &src_int32_1_2_4583_5000,
	 &src_int32_8_21_3274_5000, &src_int32_1_2_2292_5000
	}
};
struct src_stage *src_table2[10][15] = {
	{ &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_1_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_1_2_4583_5000,
	 &src_int32_0_0_0_0, &src_int32_1_2_4583_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_7_8_4583_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_2_4583_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_1_1_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_1_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_7_8_4583_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_1_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_1_1_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_1_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_2_1_2292_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_1_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_7_8_4583_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_3_1_2292_5000,
	 &src_int32_20_7_3008_5000, &src_int32_2_1_2292_5000,
	 &src_int32_1_1_0_0, &src_int32_16_7_4125_5000,
	 &src_int32_40_21_4010_5000, &src_int32_1_1_0_0,
	 &src_int32_1_1_0_0, &src_int32_20_21_4211_5000,
	 &src_int32_1_1_0_0, &src_int32_1_1_0_0,
	 &src_int32_10_21_4583_5000, &src_int32_1_1_0_0,
	 &src_int32_5_7_4583_5000, &src_int32_1_2_4583_5000
	}
};
