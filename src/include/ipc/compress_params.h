/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) AND MIT) */
/*
 *  compress_params.h - codec types and parameters for compressed data
 *  streaming interface
 *
 *  Copyright (C) 2011 Intel Corporation
 *  Authors:	Pierre-Louis Bossart <pierre-louis.bossart@linux.intel.com>
 *              Vinod Koul <vinod.koul@linux.intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The definitions in this file are derived from the OpenMAX AL version 1.1
 * and OpenMAX IL v 1.1.2 header files which contain the copyright notice below.
 *
 * Copyright (c) 2007-2010 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and/or associated documentation files (the
 * "Materials "), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 */
#ifndef __SND_COMPRESS_PARAMS_H
#define __SND_COMPRESS_PARAMS_H

#include <stdint.h>

/* AUDIO CODECS SUPPORTED */
#define MAX_NUM_CODECS 32
#define MAX_NUM_CODEC_DESCRIPTORS 32
#define MAX_NUM_BITRATES 32
#define MAX_NUM_SAMPLE_RATES 32

/* Codecs are listed linearly to allow for extensibility */
#define SND_AUDIOCODEC_PCM                   ((uint32_t) 0x00000001)
#define SND_AUDIOCODEC_MP3                   ((uint32_t) 0x00000002)
#define SND_AUDIOCODEC_AMR                   ((uint32_t) 0x00000003)
#define SND_AUDIOCODEC_AMRWB                 ((uint32_t) 0x00000004)
#define SND_AUDIOCODEC_AMRWBPLUS             ((uint32_t) 0x00000005)
#define SND_AUDIOCODEC_AAC                   ((uint32_t) 0x00000006)
#define SND_AUDIOCODEC_WMA                   ((uint32_t) 0x00000007)
#define SND_AUDIOCODEC_REAL                  ((uint32_t) 0x00000008)
#define SND_AUDIOCODEC_VORBIS                ((uint32_t) 0x00000009)
#define SND_AUDIOCODEC_FLAC                  ((uint32_t) 0x0000000A)
#define SND_AUDIOCODEC_IEC61937              ((uint32_t) 0x0000000B)
#define SND_AUDIOCODEC_G723_1                ((uint32_t) 0x0000000C)
#define SND_AUDIOCODEC_G729                  ((uint32_t) 0x0000000D)
#define SND_AUDIOCODEC_BESPOKE               ((uint32_t) 0x0000000E)
#define SND_AUDIOCODEC_ALAC                  ((uint32_t) 0x0000000F)
#define SND_AUDIOCODEC_APE                   ((uint32_t) 0x00000010)
#define SND_AUDIOCODEC_MAX                   SND_AUDIOCODEC_APE

/*
 * Profile and modes are listed with bit masks. This allows for a
 * more compact representation of fields that will not evolve
 * (in contrast to the list of codecs)
 */

#define SND_AUDIOPROFILE_PCM                 ((uint32_t) 0x00000001)

/* MP3 modes are only useful for encoders */
#define SND_AUDIOCHANMODE_MP3_MONO           ((uint32_t) 0x00000001)
#define SND_AUDIOCHANMODE_MP3_STEREO         ((uint32_t) 0x00000002)
#define SND_AUDIOCHANMODE_MP3_JOINTSTEREO    ((uint32_t) 0x00000004)
#define SND_AUDIOCHANMODE_MP3_DUAL           ((uint32_t) 0x00000008)

#define SND_AUDIOPROFILE_AMR                 ((uint32_t) 0x00000001)

/* AMR modes are only useful for encoders */
#define SND_AUDIOMODE_AMR_DTX_OFF            ((uint32_t) 0x00000001)
#define SND_AUDIOMODE_AMR_VAD1               ((uint32_t) 0x00000002)
#define SND_AUDIOMODE_AMR_VAD2               ((uint32_t) 0x00000004)

#define SND_AUDIOSTREAMFORMAT_UNDEFINED	     ((uint32_t) 0x00000000)
#define SND_AUDIOSTREAMFORMAT_CONFORMANCE    ((uint32_t) 0x00000001)
#define SND_AUDIOSTREAMFORMAT_IF1            ((uint32_t) 0x00000002)
#define SND_AUDIOSTREAMFORMAT_IF2            ((uint32_t) 0x00000004)
#define SND_AUDIOSTREAMFORMAT_FSF            ((uint32_t) 0x00000008)
#define SND_AUDIOSTREAMFORMAT_RTPPAYLOAD     ((uint32_t) 0x00000010)
#define SND_AUDIOSTREAMFORMAT_ITU            ((uint32_t) 0x00000020)

#define SND_AUDIOPROFILE_AMRWB               ((uint32_t) 0x00000001)

/* AMRWB modes are only useful for encoders */
#define SND_AUDIOMODE_AMRWB_DTX_OFF          ((uint32_t) 0x00000001)
#define SND_AUDIOMODE_AMRWB_VAD1             ((uint32_t) 0x00000002)
#define SND_AUDIOMODE_AMRWB_VAD2             ((uint32_t) 0x00000004)

#define SND_AUDIOPROFILE_AMRWBPLUS           ((uint32_t) 0x00000001)

#define SND_AUDIOPROFILE_AAC                 ((uint32_t) 0x00000001)

/* AAC modes are required for encoders and decoders */
#define SND_AUDIOMODE_AAC_MAIN               ((uint32_t) 0x00000001)
#define SND_AUDIOMODE_AAC_LC                 ((uint32_t) 0x00000002)
#define SND_AUDIOMODE_AAC_SSR                ((uint32_t) 0x00000004)
#define SND_AUDIOMODE_AAC_LTP                ((uint32_t) 0x00000008)
#define SND_AUDIOMODE_AAC_HE                 ((uint32_t) 0x00000010)
#define SND_AUDIOMODE_AAC_SCALABLE           ((uint32_t) 0x00000020)
#define SND_AUDIOMODE_AAC_ERLC               ((uint32_t) 0x00000040)
#define SND_AUDIOMODE_AAC_LD                 ((uint32_t) 0x00000080)
#define SND_AUDIOMODE_AAC_HE_PS              ((uint32_t) 0x00000100)
#define SND_AUDIOMODE_AAC_HE_MPS             ((uint32_t) 0x00000200)

/* AAC formats are required for encoders and decoders */
#define SND_AUDIOSTREAMFORMAT_MP2ADTS        ((uint32_t) 0x00000001)
#define SND_AUDIOSTREAMFORMAT_MP4ADTS        ((uint32_t) 0x00000002)
#define SND_AUDIOSTREAMFORMAT_MP4LOAS        ((uint32_t) 0x00000004)
#define SND_AUDIOSTREAMFORMAT_MP4LATM        ((uint32_t) 0x00000008)
#define SND_AUDIOSTREAMFORMAT_ADIF           ((uint32_t) 0x00000010)
#define SND_AUDIOSTREAMFORMAT_MP4FF          ((uint32_t) 0x00000020)
#define SND_AUDIOSTREAMFORMAT_RAW            ((uint32_t) 0x00000040)

#define SND_AUDIOPROFILE_WMA7                ((uint32_t) 0x00000001)
#define SND_AUDIOPROFILE_WMA8                ((uint32_t) 0x00000002)
#define SND_AUDIOPROFILE_WMA9                ((uint32_t) 0x00000004)
#define SND_AUDIOPROFILE_WMA10               ((uint32_t) 0x00000008)
#define SND_AUDIOPROFILE_WMA9_PRO            ((uint32_t) 0x00000010)
#define SND_AUDIOPROFILE_WMA9_LOSSLESS       ((uint32_t) 0x00000020)
#define SND_AUDIOPROFILE_WMA10_LOSSLESS      ((uint32_t) 0x00000040)

#define SND_AUDIOMODE_WMA_LEVEL1             ((uint32_t) 0x00000001)
#define SND_AUDIOMODE_WMA_LEVEL2             ((uint32_t) 0x00000002)
#define SND_AUDIOMODE_WMA_LEVEL3             ((uint32_t) 0x00000004)
#define SND_AUDIOMODE_WMA_LEVEL4             ((uint32_t) 0x00000008)
#define SND_AUDIOMODE_WMAPRO_LEVELM0         ((uint32_t) 0x00000010)
#define SND_AUDIOMODE_WMAPRO_LEVELM1         ((uint32_t) 0x00000020)
#define SND_AUDIOMODE_WMAPRO_LEVELM2         ((uint32_t) 0x00000040)
#define SND_AUDIOMODE_WMAPRO_LEVELM3         ((uint32_t) 0x00000080)

#define SND_AUDIOSTREAMFORMAT_WMA_ASF        ((uint32_t) 0x00000001)
/*
 * Some implementations strip the ASF header and only send ASF packets
 * to the DSP
 */
#define SND_AUDIOSTREAMFORMAT_WMA_NOASF_HDR  ((uint32_t) 0x00000002)

#define SND_AUDIOPROFILE_REALAUDIO           ((uint32_t) 0x00000001)

#define SND_AUDIOMODE_REALAUDIO_G2           ((uint32_t) 0x00000001)
#define SND_AUDIOMODE_REALAUDIO_8            ((uint32_t) 0x00000002)
#define SND_AUDIOMODE_REALAUDIO_10           ((uint32_t) 0x00000004)
#define SND_AUDIOMODE_REALAUDIO_SURROUND     ((uint32_t) 0x00000008)

#define SND_AUDIOPROFILE_VORBIS              ((uint32_t) 0x00000001)

#define SND_AUDIOMODE_VORBIS                 ((uint32_t) 0x00000001)

#define SND_AUDIOPROFILE_FLAC                ((uint32_t) 0x00000001)

/*
 * Define quality levels for FLAC encoders, from LEVEL0 (fast)
 * to LEVEL8 (best)
 */
#define SND_AUDIOMODE_FLAC_LEVEL0            ((uint32_t) 0x00000001)
#define SND_AUDIOMODE_FLAC_LEVEL1            ((uint32_t) 0x00000002)
#define SND_AUDIOMODE_FLAC_LEVEL2            ((uint32_t) 0x00000004)
#define SND_AUDIOMODE_FLAC_LEVEL3            ((uint32_t) 0x00000008)
#define SND_AUDIOMODE_FLAC_LEVEL4            ((uint32_t) 0x00000010)
#define SND_AUDIOMODE_FLAC_LEVEL5            ((uint32_t) 0x00000020)
#define SND_AUDIOMODE_FLAC_LEVEL6            ((uint32_t) 0x00000040)
#define SND_AUDIOMODE_FLAC_LEVEL7            ((uint32_t) 0x00000080)
#define SND_AUDIOMODE_FLAC_LEVEL8            ((uint32_t) 0x00000100)

#define SND_AUDIOSTREAMFORMAT_FLAC           ((uint32_t) 0x00000001)
#define SND_AUDIOSTREAMFORMAT_FLAC_OGG       ((uint32_t) 0x00000002)

/* IEC61937 payloads without CUVP and preambles */
#define SND_AUDIOPROFILE_IEC61937            ((uint32_t) 0x00000001)
/* IEC61937 with S/PDIF preambles+CUVP bits in 32-bit containers */
#define SND_AUDIOPROFILE_IEC61937_SPDIF      ((uint32_t) 0x00000002)

/*
 * IEC modes are mandatory for decoders. Format autodetection
 * will only happen on the DSP side with mode 0. The PCM mode should
 * not be used, the PCM codec should be used instead.
 */
#define SND_AUDIOMODE_IEC_REF_STREAM_HEADER  ((uint32_t) 0x00000000)
#define SND_AUDIOMODE_IEC_LPCM		     ((uint32_t) 0x00000001)
#define SND_AUDIOMODE_IEC_AC3		     ((uint32_t) 0x00000002)
#define SND_AUDIOMODE_IEC_MPEG1		     ((uint32_t) 0x00000004)
#define SND_AUDIOMODE_IEC_MP3		     ((uint32_t) 0x00000008)
#define SND_AUDIOMODE_IEC_MPEG2		     ((uint32_t) 0x00000010)
#define SND_AUDIOMODE_IEC_AACLC		     ((uint32_t) 0x00000020)
#define SND_AUDIOMODE_IEC_DTS		     ((uint32_t) 0x00000040)
#define SND_AUDIOMODE_IEC_ATRAC		     ((uint32_t) 0x00000080)
#define SND_AUDIOMODE_IEC_SACD		     ((uint32_t) 0x00000100)
#define SND_AUDIOMODE_IEC_EAC3		     ((uint32_t) 0x00000200)
#define SND_AUDIOMODE_IEC_DTS_HD	     ((uint32_t) 0x00000400)
#define SND_AUDIOMODE_IEC_MLP		     ((uint32_t) 0x00000800)
#define SND_AUDIOMODE_IEC_DST		     ((uint32_t) 0x00001000)
#define SND_AUDIOMODE_IEC_WMAPRO	     ((uint32_t) 0x00002000)
#define SND_AUDIOMODE_IEC_REF_CXT            ((uint32_t) 0x00004000)
#define SND_AUDIOMODE_IEC_HE_AAC	     ((uint32_t) 0x00008000)
#define SND_AUDIOMODE_IEC_HE_AAC2	     ((uint32_t) 0x00010000)
#define SND_AUDIOMODE_IEC_MPEG_SURROUND	     ((uint32_t) 0x00020000)

#define SND_AUDIOPROFILE_G723_1              ((uint32_t) 0x00000001)

#define SND_AUDIOMODE_G723_1_ANNEX_A         ((uint32_t) 0x00000001)
#define SND_AUDIOMODE_G723_1_ANNEX_B         ((uint32_t) 0x00000002)
#define SND_AUDIOMODE_G723_1_ANNEX_C         ((uint32_t) 0x00000004)

#define SND_AUDIOPROFILE_G729                ((uint32_t) 0x00000001)

#define SND_AUDIOMODE_G729_ANNEX_A           ((uint32_t) 0x00000001)
#define SND_AUDIOMODE_G729_ANNEX_B           ((uint32_t) 0x00000002)

/* <FIXME: multichannel encoders aren't supported for now. Would need
   an additional definition of channel arrangement> */

/* VBR/CBR definitions */
#define SND_RATECONTROLMODE_CONSTANTBITRATE  ((uint32_t) 0x00000001)
#define SND_RATECONTROLMODE_VARIABLEBITRATE  ((uint32_t) 0x00000002)

/* Encoder options */

struct snd_enc_wma {
	uint32_t super_block_align; /* WMA Type-specific data */
};


/**
 * struct snd_enc_vorbis
 *
 * \@quality: Sets encoding quality to n, between -1 (low) and 10 (high).
 * In the default mode of operation, the quality level is 3.
 * Normal quality range is 0 - 10.
 * \@managed: Boolean. Set  bitrate  management  mode. This turns off the
 * normal VBR encoding, but allows hard or soft bitrate constraints to be
 * enforced by the encoder. This mode can be slower, and may also be
 * lower quality. It is primarily useful for streaming.
 * \@max_bit_rate: Enabled only if managed is TRUE
 * \@min_bit_rate: Enabled only if managed is TRUE
 * \@downmix: Boolean. Downmix input from stereo to mono (has no effect on
 * non-stereo streams). Useful for lower-bitrate encoding.
 *
 * These options were extracted from the OpenMAX IL spec and Gstreamer vorbisenc
 * properties
 *
 * For best quality users should specify VBR mode and set quality levels.
 */

struct snd_enc_vorbis {
	int32_t quality;
	uint32_t managed;
	uint32_t max_bit_rate;
	uint32_t min_bit_rate;
	uint32_t downmix;
} __attribute__((packed, aligned(4)));


/**
 * struct snd_enc_real
 *
 * \@quant_bits: number of coupling quantization bits in the stream
 * \@start_region: coupling start region in the stream
 * \@num_regions: number of regions value
 *
 * These options were extracted from the OpenMAX IL spec
 */

struct snd_enc_real {
	uint32_t quant_bits;
	uint32_t start_region;
	uint32_t num_regions;
} __attribute__((packed, aligned(4)));

/**
 * struct snd_enc_flac
 *
 * \@num: serial number, valid only for OGG formats
 *	needs to be set by application
 * \@gain: Add replay gain tags
 *
 * These options were extracted from the FLAC online documentation
 * at http://flac.sourceforge.net/documentation_tools_flac.html
 *
 * To make the API simpler, it is assumed that the user will select quality
 * profiles. Additional options that affect encoding quality and speed can
 * be added at a later stage if needed.
 *
 * By default the Subset format is used by encoders.
 *
 * TAGS such as pictures, etc, cannot be handled by an offloaded encoder and are
 * not supported in this API.
 */

struct snd_enc_flac {
	uint32_t num;
	uint32_t gain;
} __attribute__((packed, aligned(4)));

struct snd_enc_generic {
	uint32_t bw;	/* encoder bandwidth */
	int32_t reserved[15];	/* Can be used for SND_AUDIOCODEC_BESPOKE */
} __attribute__((packed, aligned(4)));

struct snd_dec_flac {
	uint16_t sample_size;
	uint16_t min_blk_size;
	uint16_t max_blk_size;
	uint16_t min_frame_size;
	uint16_t max_frame_size;
	uint16_t reserved;
} __attribute__((packed, aligned(4)));

struct snd_dec_wma {
	uint32_t encoder_option;
	uint32_t adv_encoder_option;
	uint32_t adv_encoder_option2;
	uint32_t reserved;
} __attribute__((packed, aligned(4)));

struct snd_dec_alac {
	uint32_t frame_length;
	uint8_t compatible_version;
	uint8_t pb;
	uint8_t mb;
	uint8_t kb;
	uint32_t max_run;
	uint32_t max_frame_bytes;
} __attribute__((packed, aligned(4)));

struct snd_dec_ape {
	uint16_t compatible_version;
	uint16_t compression_level;
	uint32_t format_flags;
	uint32_t blocks_per_frame;
	uint32_t final_frame_blocks;
	uint32_t total_frames;
	uint32_t seek_table_present;
} __attribute__((packed, aligned(4)));

union snd_codec_options {
	struct snd_enc_wma wma;
	struct snd_enc_vorbis vorbis;
	struct snd_enc_real real;
	struct snd_enc_flac flac;
	struct snd_enc_generic generic;
	struct snd_dec_flac flac_d;
	struct snd_dec_wma wma_d;
	struct snd_dec_alac alac_d;
	struct snd_dec_ape ape_d;
} __attribute__((packed, aligned(4)));

/** struct snd_codec_desc - description of codec capabilities
 *
 * \@max_ch: Maximum number of audio channels
 * \@sample_rates: Sampling rates in Hz, use values like 48000 for this
 * \@num_sample_rates: Number of valid values in sample_rates array
 * \@bit_rate: Indexed array containing supported bit rates
 * \@num_bitrates: Number of valid values in bit_rate array
 * \@rate_control: value is specified by SND_RATECONTROLMODE defines.
 * \@profiles: Supported profiles. See SND_AUDIOPROFILE defines.
 * \@modes: Supported modes. See SND_AUDIOMODE defines
 * \@formats: Supported formats. See SND_AUDIOSTREAMFORMAT defines
 * \@min_buffer: Minimum buffer size handled by codec implementation
 * \@reserved: reserved for future use
 *
 * This structure provides a scalar value for profiles, modes and stream
 * format fields.
 * If an implementation supports multiple combinations, they will be listed as
 * codecs with different descriptors, for example there would be 2 descriptors
 * for AAC-RAW and AAC-ADTS.
 * This entails some redundancy but makes it easier to avoid invalid
 * configurations.
 *
 */

struct snd_codec_desc {
	uint32_t max_ch;
	uint32_t sample_rates[MAX_NUM_SAMPLE_RATES];
	uint32_t num_sample_rates;
	uint32_t bit_rate[MAX_NUM_BITRATES];
	uint32_t num_bitrates;
	uint32_t rate_control;
	uint32_t profiles;
	uint32_t modes;
	uint32_t formats;
	uint32_t min_buffer;
	uint32_t reserved[15];
} __attribute__((packed, aligned(4)));

/**
 * struct snd_codec
 *
 * \@id: Identifies the supported audio encoder/decoder.
 *		See SND_AUDIOCODEC macros.
 * \@ch_in: Number of input audio channels
 * \@ch_out: Number of output channels. In case of contradiction between
 *		this field and the channelMode field, the channelMode field
 *		overrides.
 * \@sample_rate: Audio sample rate of input data in Hz, use values like 48000
 *		for this.
 * \@bit_rate: Bitrate of encoded data. May be ignored by decoders
 * \@rate_control: Encoding rate control. See SND_RATECONTROLMODE defines.
 *               Encoders may rely on profiles for quality levels.
 *		 May be ignored by decoders.
 * \@profile: Mandatory for encoders, can be mandatory for specific
 *		decoders as well. See SND_AUDIOPROFILE defines.
 * \@level: Supported level (Only used by WMA at the moment)
 * \@ch_mode: Channel mode for encoder. See SND_AUDIOCHANMODE defines
 * \@format: Format of encoded bistream. Mandatory when defined.
 *		See SND_AUDIOSTREAMFORMAT defines.
 * \@align: Block alignment in bytes of an audio sample.
 *		Only required for PCM or IEC formats.
 * \@options: encoder-specific settings
 * \@reserved: reserved for future use
 */
struct snd_codec {
	uint32_t id;
	uint32_t ch_in;
	uint32_t ch_out;
	uint32_t sample_rate;
	uint32_t bit_rate;
	uint32_t rate_control;
	uint32_t profile;
	uint32_t level;
	uint32_t ch_mode;
	uint32_t format;
	uint32_t align;
	union snd_codec_options options;
	uint32_t reserved[3];
} __attribute__((packed, aligned(4)));

#endif
