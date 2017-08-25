/* SRC conversions */
#include <reef/audio/coefficients/src/src_int16_1_3_4375_5000.h>
#include <reef/audio/coefficients/src/src_int16_3_1_4375_5000.h>
#include <reef/audio/coefficients/src/src_int16_7_8_4375_5000.h>
#include <reef/audio/coefficients/src/src_int16_8_7_4375_5000.h>
#include <reef/audio/coefficients/src/src_int16_20_21_4020_5000.h>
#include <reef/audio/coefficients/src/src_int16_21_20_4020_5000.h>

/* SRC table */
int16_t fir_one = 16384;
struct src_stage src_int16_1_1_0_0 =  { 0, 0, 1, 1, 1, 1, 1, 0, -1, &fir_one };
struct src_stage src_int16_0_0_0_0 =  { 0, 0, 0, 0, 0, 0, 0, 0,  0, &fir_one };
int src_in_fs[3] = { 16000, 44100, 48000};
int src_out_fs[3] = { 16000, 44100, 48000};
struct src_stage *src_table1[3][3] = {
	{ &src_int16_1_1_0_0, &src_int16_0_0_0_0,
	 &src_int16_1_3_4375_5000},
	{ &src_int16_0_0_0_0,
	 &src_int16_1_1_0_0, &src_int16_21_20_4020_5000
	},
	{ &src_int16_3_1_4375_5000, &src_int16_8_7_4375_5000,
	 &src_int16_1_1_0_0}
};
struct src_stage *src_table2[3][3] = {
	{ &src_int16_1_1_0_0, &src_int16_0_0_0_0,
	 &src_int16_1_1_0_0},
	{ &src_int16_0_0_0_0,
	 &src_int16_1_1_0_0, &src_int16_7_8_4375_5000
	},
	{ &src_int16_1_1_0_0, &src_int16_20_21_4020_5000,
	 &src_int16_1_1_0_0}
};
