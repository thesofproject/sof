/* SRC conversions */
#include <reef/audio/coefficients/src/src_tiny_int16_1_2_3750_5100.h>
#include <reef/audio/coefficients/src/src_tiny_int16_1_3_1875_5100.h>
#include <reef/audio/coefficients/src/src_tiny_int16_1_3_3750_5100.h>
#include <reef/audio/coefficients/src/src_tiny_int16_2_1_3750_5100.h>
#include <reef/audio/coefficients/src/src_tiny_int16_2_3_3750_5100.h>
#include <reef/audio/coefficients/src/src_tiny_int16_3_1_1875_5100.h>
#include <reef/audio/coefficients/src/src_tiny_int16_3_1_3750_5100.h>
#include <reef/audio/coefficients/src/src_tiny_int16_3_2_3750_5100.h>
#include <reef/audio/coefficients/src/src_tiny_int16_7_8_3750_5100.h>
#include <reef/audio/coefficients/src/src_tiny_int16_8_7_3750_5100.h>
#include <reef/audio/coefficients/src/src_tiny_int16_20_21_3445_5100.h>
#include <reef/audio/coefficients/src/src_tiny_int16_21_20_3445_5100.h>

/* SRC table */
int16_t fir_one = 16384;
struct src_stage src_int16_1_1_0_0 =  { 0, 0, 1, 1, 1, 1, 1, 0, -1, &fir_one };
struct src_stage src_int16_0_0_0_0 =  { 0, 0, 0, 0, 0, 0, 0, 0,  0, &fir_one };
int src_in_fs[6] = { 8000, 16000, 24000, 32000, 44100, 48000};
int src_out_fs[6] = { 8000, 16000, 24000, 32000, 44100, 48000};
struct src_stage *src_table1[6][6] = {
	{ &src_int16_1_1_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_1_3_1875_5100
	},
	{ &src_int16_0_0_0_0, &src_int16_1_1_0_0,
	 &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_1_3_3750_5100
	},
	{ &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_1_1_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_1_2_3750_5100
	},
	{ &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_1_1_0_0,
	 &src_int16_0_0_0_0, &src_int16_2_3_3750_5100
	},
	{ &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_1_1_0_0, &src_int16_21_20_3445_5100
	},
	{ &src_int16_2_1_3750_5100, &src_int16_3_1_3750_5100,
	 &src_int16_2_1_3750_5100, &src_int16_3_2_3750_5100,
	 &src_int16_8_7_3750_5100, &src_int16_1_1_0_0
	}
};
struct src_stage *src_table2[6][6] = {
	{ &src_int16_1_1_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_1_2_3750_5100
	},
	{ &src_int16_0_0_0_0, &src_int16_1_1_0_0,
	 &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_1_1_0_0
	},
	{ &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_1_1_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_1_1_0_0
	},
	{ &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_1_1_0_0,
	 &src_int16_0_0_0_0, &src_int16_1_1_0_0
	},
	{ &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_1_1_0_0, &src_int16_7_8_3750_5100
	},
	{ &src_int16_3_1_1875_5100, &src_int16_1_1_0_0,
	 &src_int16_1_1_0_0, &src_int16_1_1_0_0,
	 &src_int16_20_21_3445_5100, &src_int16_1_1_0_0
	}
};
