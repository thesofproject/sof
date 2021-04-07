/*
 * Copyright (c) 2006-2020 Cadence Design Systems, Inc.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#ifndef __XA_AAC_DEC_API_H__
#define __XA_AAC_DEC_API_H__

/* aac_dec-specific configuration parameters */
enum xa_config_param_aac_dec {
  XA_AACDEC_CONFIG_PARAM_BDOWNSAMPLE          = 0,    /* Applicable only for aac*plus* libraries */
  XA_AACDEC_CONFIG_PARAM_BBITSTREAMDOWNMIX    = 1,    /* Applicable only for aac*plus* libraries */
  XA_AACDEC_CONFIG_PARAM_EXTERNAL_SAMPLERATE  = 2,
#define XA_AACDEC_CONFIG_PARAM_EXTERNALSAMPLINGRATE XA_AACDEC_CONFIG_PARAM_EXTERNAL_SAMPLERATE // Renamed, maintained for backward comapatible
  XA_AACDEC_CONFIG_PARAM_EXTERNALBSFORMAT     = 3,
  XA_AACDEC_CONFIG_PARAM_TO_STEREO            = 4,
  XA_AACDEC_CONFIG_PARAM_OUT_SAMPLERATE       = 5,
#define  XA_AACDEC_CONFIG_PARAM_SAMP_FREQ           XA_AACDEC_CONFIG_PARAM_OUT_SAMPLERATE // Renamed, maintained for backward comapatible
  XA_AACDEC_CONFIG_PARAM_NUM_CHANNELS         = 6,
  XA_AACDEC_CONFIG_PARAM_PCM_WDSZ             = 7,
  XA_AACDEC_CONFIG_PARAM_SBR_TYPE             = 8,
  XA_AACDEC_CONFIG_PARAM_AAC_SAMPLERATE       = 9,
  XA_AACDEC_CONFIG_PARAM_DATA_RATE            = 10,
  XA_AACDEC_CONFIG_PARAM_OUTNCHANS            = 11,
  XA_AACDEC_CONFIG_PARAM_CHANROUTING          = 12,
  XA_AACDEC_CONFIG_PARAM_SBR_SIGNALING        = 13,    /* Applicable only for aac*plus* libraries */
  XA_AACDEC_CONFIG_PARAM_CHANMAP              = 14,
  XA_AACDEC_CONFIG_PARAM_ACMOD                = 15,
  XA_AACDEC_CONFIG_PARAM_AAC_FORMAT           = 16,
  XA_AACDEC_CONFIG_PARAM_ZERO_UNUSED_CHANS    = 17,
  XA_AACDEC_CONFIG_PARAM_DECODELAYERS         = 18,   /* Depricated, no more implemented */
  XA_AACDEC_CONFIG_PARAM_EXTERNALCHCONFIG     = 19,   /* Depricated, no more implemented */
  XA_AACDEC_CONFIG_PARAM_RAW_AU_SIDEINFO      = 20,   /* For DAB-plus only */
  XA_AACDEC_CONFIG_PARAM_EXTERNALBITRATE      = 21,   /* For DAB-plus only */
  XA_AACDEC_CONFIG_PARAM_PAD_SIZE             = 22,   /* For DAB-plus only */
  XA_AACDEC_CONFIG_PARAM_PAD_PTR              = 23,   /* For DAB-plus only */
  XA_AACDEC_CONFIG_PARAM_MPEGSURR_PRESENT     = 24,   /* For DAB-plus only */
  XA_AACDEC_CONFIG_PARAM_METADATASTRUCT_PTR   = 25,   /* Only if Audio MetaData support is present for the library */
  XA_AACDEC_CONFIG_PARAM_ASCONFIGSTRUCT_PTR   = 26,   /* Only if Audio MetaData support is present for the library */
  XA_AACDEC_CONFIG_PARAM_LIMITBANDWIDTH       = 27,   /* Depricated, no more implemented */
  XA_AACDEC_CONFIG_PARAM_PCE_STATUS           = 28,   /* for Loas build only */
  XA_AACDEC_CONFIG_PARAM_DWNMIX_METADATA      = 29,   /* for Loas build only */
  XA_AACDEC_CONFIG_PARAM_MPEG_ID              = 30,   /* Applicable only for adts streams */
  XA_AACDEC_CONFIG_PARAM_DWNMIX_LEVEL_DVB     = 31    /* for Loas build only */
  /* DRC and PRL information as per ISO/IEC 14496.3 */
  /* PRL Parametbers */
  ,XA_AACDEC_CONFIG_PARAM_ENABLE_APPLY_PRL     = 32     /* for Loas build only */
  ,XA_AACDEC_CONFIG_PARAM_TARGET_LEVEL         = 33     /* for Loas build only */
  ,XA_AACDEC_CONFIG_PARAM_PROG_REF_LEVEL       = 34     /* for Loas build only */
  /* DRC Parametbers */
  ,XA_AACDEC_CONFIG_PARAM_ENABLE_APPLY_DRC     = 35     /* for Loas build only */
  ,XA_AACDEC_CONFIG_PARAM_DRC_COMPRESS_FAC     = 36     /* for Loas build only */
  ,XA_AACDEC_CONFIG_PARAM_DRC_BOOST_FAC        = 37     /* for Loas build only */
  ,XA_AACDEC_CONFIG_PARAM_DRC_EXT_PRESENT      = 38
  ,XA_AACDEC_CONFIG_PARAM_ORIGINAL_OR_COPY     = 39     /* for ADTS and ADIF files only */
  ,XA_AACDEC_CONFIG_PARAM_COPYRIGHT_ID_PTR     = 40     /* for ADTS and ADIF files only */
  ,XA_AACDEC_CONFIG_PARAM_PARSED_DRC_INFO      = 41     /* applicable only for aacmch* builds */
  ,XA_AACDEC_CONFIG_PARAM_INPUT_BITOFFSET      = 42
  ,XA_AACDEC_CONFIG_PARAM_ENABLE_FRAME_BY_FRAME_DECODE      = 43
  ,XA_AACDEC_CONFIG_PARAM_CONCEALMENT_FADE_OUT_FRAMES       = 44
  ,XA_AACDEC_CONFIG_PARAM_CONCEALMENT_MUTE_RELEASE_FRAMES   = 45
  ,XA_AACDEC_CONFIG_PARAM_CONCEALMENT_FADE_IN_FRAMES        = 46	 
  ,XA_AACDEC_CONFIG_PARAM_MPEG4_AMENDMENT4_ENABLE           = 47
  ,XA_AACDEC_CONFIG_PARAM_CHANNEL_CONFIG_INFO_FROM_PCE      = 48
  ,XA_AACDEC_CONFIG_PARAM_RESET_STATE_ON_SYNC_LOSS_ERROR    = 49
};

/* Types of channel modes (acmod) */
/*
======================================================================================
Acro.   Expansion                   MPEG2/4 CH. Mapping        MPEG4 AMD.4 CH. Mapping
=======================================================================================
CF      Center Front                CF        ->  C             CF    -> C
LF      Left Front                  LF or LCF ->  L             LF    -> L
RF      Right Front                 RF or RCF ->  R             RF    -> R
LCF     Left Center Front           LS or LOF ->  l             LS    -> l
RCF     Right Center Front          RS or ROF ->  r             RS    -> r
rs      Rear Surround               rs        ->  Cs            rc    -> Cs
LSR     Left Surround Rear          LSR       ->  Sbl           rsl   -> Sbl
RSR     Right Surround Rear         RSR       ->  Rsr           rsr   -> Rsr
LOF     Left Outside Front          LFE       ->  LFE           LFE   -> LFE
ROF     Right Outside Front                                     LFC   -> Sbl
LFE     Low Frequency Effects                                   RFC   -> Sbr
rc      Rear Center                                             LFVH  -> Sbl
rsl     Rear Surround Left                                      RFVH  -> Sbr
rsr     Rear Surround Right             
LFVH    Left Front Vertical Height              
RFVH    Right Front Vertical Height             
LFC     Left Front Center               
RFC     Right Front Center              
=======================================================================================
*/

typedef enum {
    //Input Channel Map Enum                        Tensilica Convention (used for routing)         MPEG2/4                                         MPEG4 AMD. 4
    XA_AACDEC_CHANNELMODE_UNDEFINED = 0,            // undefined        
    XA_AACDEC_CHANNELMODE_MONO = 1,                 // mono (1/0 )                                  CF (1)                                          == SAME ==
    XA_AACDEC_CHANNELMODE_PARAMETRIC_STEREO = 2,    // parametric stereo (aacPlus v2 only)          LF, RF (2)                                      == SAME ==
    XA_AACDEC_CHANNELMODE_DUAL_CHANNEL = 3,         // dual mono (1/0 + 1/0)        
    XA_AACDEC_CHANNELMODE_STEREO = 4,               // stereo (2/0)                                 LF, RF (2)                                      == SAME ==
    XA_AACDEC_CHANNELMODE_3_CHANNEL_FRONT = 5,      // C, L, R (3/0)                                CF, LF, RF (3)                                  == SAME ==
    XA_AACDEC_CHANNELMODE_3_CHANNEL_SURR = 6,       // L, R, l (2/1)        
    XA_AACDEC_CHANNELMODE_4_CHANNEL_2SURR = 7,      // L, R, l, r (2/2)     
    XA_AACDEC_CHANNELMODE_4_CHANNEL_1SURR = 8,      // C, L R, Cs (3/0/1)                           CF, LF, RF, rs (4)                              CF, LF, RF, rc (4)
    XA_AACDEC_CHANNELMODE_5_CHANNEL = 9,            //  C, L, R, l, r (3/2)                         CF, LF, RF, LSR, RSR (5)                        CF, LF, RF, LS, RS (5)
    XA_AACDEC_CHANNELMODE_6_CHANNEL = 10,           // C, L, R, l, r, Cs (3/2/1)        
    XA_AACDEC_CHANNELMODE_7_CHANNEL = 11,           // C, L, R, l, r, Sbl, Sbr (3/2/2)      
    XA_AACDEC_CHANNELMODE_7_CHANNEL_FRONT = 11,     // C, Sbl, Sbr, L, R, l, r (3/2/2)      
    XA_AACDEC_CHANNELMODE_7_CHANNEL_BACK = 20,      // C, L, R, l, r, Sbl, Sbr (3/2/2)      
    XA_AACDEC_CHANNELMODE_7_CHANNEL_TOP = 21,       // C, L, R, l, r, Sbl, Sbr (3/2/2)      
    XA_AACDEC_CHANNELMODE_2_1_STEREO = 12,          // L, R, LFE (2/0.1)       
    XA_AACDEC_CHANNELMODE_3_1_CHANNEL_FRONT = 13,   // C, L, R, LFE (3/0.1)     
    XA_AACDEC_CHANNELMODE_3_1_CHANNEL_SURR = 14,    // L, R, Cs, LFE (2/0/1.1)      
    XA_AACDEC_CHANNELMODE_4_1_CHANNEL_2SURR = 15,   // L, R, Ls, Rs, LFE (2/2.1)        
    XA_AACDEC_CHANNELMODE_4_1_CHANNEL_1SURR = 16,   // C, L, R, Cs, LFE (3/0/1.1)       
    XA_AACDEC_CHANNELMODE_5_1_CHANNEL = 17,         // C, L, R, l, r, LFE (5.1 mode)                CF, LF, RF, LSR, RSR, LFE (6)                   CF, LF, RF, LS, RS, LFE (6)
    XA_AACDEC_CHANNELMODE_6_1_CHANNEL = 18,         // C, L, R, l, r, Cs, LFE (3/2/1.1)             NA                                              CF, LF, RF, LS, RS, rc, LFE (11)
    XA_AACDEC_CHANNELMODE_7_1_CHANNEL = 19,         // C, L, R, l, r, Sbl, Sbr, LFE (7.1 mode)      CF, LCF, RCF, LOF, ROF, LSR, RSR, LFE (7)       NA
    XA_AACDEC_CHANNELMODE_7_1_CHANNEL_FRONT = 19,   // C, Sbl, Sbr, L, R, l, r, LFE (7.1 mode)      NA                                              CF, LFC, RFC, LF, RF, LS, RS, LFE (7)
    XA_AACDEC_CHANNELMODE_7_1_CHANNEL_BACK = 22,    // C, L, R, l, r, Sbl, Sbr, LFE (7.1 mode)      NA                                              CF, LF, RF, LS, RS, rsl, rsr, LFE (12)
    XA_AACDEC_CHANNELMODE_7_1_CHANNEL_TOP = 23,     // C, L, R, l, r, LFE, Sbl, Sbr (7.1 mode)      NA                                              CF, LF, RF, LS, RS, LFE, LFVH, RFVH(14)

} XA_AACDEC_CHANNELMODE;

/* Types of bitstreams */
typedef enum {
  /* The bitstream type has not (yet) been successfully determined. */
  XA_AACDEC_EBITSTREAM_TYPE_UNKNOWN = 0,
  /* ADIF is an unsynced, unframed format. Errors in the bitstream cannot always
     be detected, and when they occur, no further parsing is possible. Avoid ADIF at
     all costs. */
  XA_AACDEC_EBITSTREAM_TYPE_AAC_ADIF = 1,
  /* ADTS is a simple synced framing format similar to MPEG layer-3. */
  XA_AACDEC_EBITSTREAM_TYPE_AAC_ADTS = 2,
  /* LATM, with in-band config. This format cannot be detected by the library;
     it needs to be signaled explicitely. */
  XA_AACDEC_EBITSTREAM_TYPE_AAC_LATM = 3,
  /* LATM, with out of band config. This format is not supported. */
  XA_AACDEC_EBITSTREAM_TYPE_AAC_LATM_OUTOFBAND_CONFIG = 4,
  /* Low overhead audio stream. */
  XA_AACDEC_EBITSTREAM_TYPE_AAC_LOAS = 5,

  /* Raw bitstream. This format cannot be detected by the library;
     it needs to be signaled explicitly. */
  XA_AACDEC_EBITSTREAM_TYPE_AAC_RAW = 6,

  /* Raw DAB+ bitstream. It needs sideInfo for every frame for error recovery */
  XA_AACDEC_EBITSTREAM_TYPE_DABPLUS_RAW_SIDEINFO = 8,

  /* DAB+ audio superframe bitstream */
  XA_AACDEC_EBITSTREAM_TYPE_DABPLUS = 9

} XA_AACDEC_EBITSTREAM_TYPE;

/* commands */
#include "xa_apicmd_standards.h"

/* error codes */
#include "xa_error_standards.h"

#define XA_CODEC_AAC_DEC 3

/* aac_dec-specific error codes */
/*****************************************************************************/
/* Class 0: API Errors                                                       */
/*****************************************************************************/
/* Non Fatal Errors */
enum xa_error_nonfatal_api_aac_dec {
  XA_AACDEC_API_NONFATAL_CMD_TYPE_NOT_SUPPORTED = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_api, XA_CODEC_AAC_DEC, 0),
  XA_AACDEC_API_NONFATAL_INVALID_API_SEQ        = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_api, XA_CODEC_AAC_DEC, 1)
};

enum xa_error_fatal_api_aac_dec {
  XA_AACDEC_API_FATAL_INVALID_API_SEQ        = XA_ERROR_CODE(xa_severity_fatal, xa_class_api, XA_CODEC_AAC_DEC, 4)
};

/* Fatal Errors */
/* (none) */

/*****************************************************************************/
/* Class 1: Configuration Errors                                             */
/*****************************************************************************/
/* Nonfatal Errors */
enum xa_error_nonfatal_config_aac_dec {
  XA_AACDEC_CONFIG_NONFATAL_PARAMS_NOT_SET                      = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_config, XA_CODEC_AAC_DEC, 0),
  XA_AACDEC_CONFIG_NONFATAL_DATA_RATE_NOT_SET                   = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_config, XA_CODEC_AAC_DEC, 1),
  XA_AACDEC_CONFIG_NONFATAL_PARTIAL_CHANROUTING                 = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_config, XA_CODEC_AAC_DEC, 2)
  ,XA_AACDEC_CONFIG_NONFATAL_INVALID_GEN_STRM_POS               = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_config, XA_CODEC_AAC_DEC, 3)
  ,XA_AACDEC_CONFIG_NONFATAL_CPID_NOT_PRESENT                   = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_config, XA_CODEC_AAC_DEC, 4)
  ,XA_AACDEC_CONFIG_NONFATAL_INVALID_PRL_PARAMS                 = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_config, XA_CODEC_AAC_DEC, 5)
  ,XA_AACDEC_CONFIG_NONFATAL_INVALID_DRC_PARAMS                 = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_config, XA_CODEC_AAC_DEC, 6)
  ,XA_AACDEC_CONFIG_NONFATAL_INVALID_PARAM_VALUE                = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_config, XA_CODEC_AAC_DEC, 7)
};
/* Fatal Errors */
enum xa_error_fatal_config_aac_dec {
  XA_AACDEC_CONFIG_FATAL_INVALID_BDOWNSAMPLE          = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_AAC_DEC, 0),
  XA_AACDEC_CONFIG_FATAL_INVALID_BBITSTREAMDOWNMIX    = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_AAC_DEC, 1),
  XA_AACDEC_CONFIG_FATAL_INVALID_EXTERNALSAMPLINGRATE = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_AAC_DEC, 2),
  XA_AACDEC_CONFIG_FATAL_INVALID_EXTERNALBSFORMAT     = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_AAC_DEC, 3),
  XA_AACDEC_CONFIG_FATAL_INVALID_TO_STEREO            = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_AAC_DEC, 4),
  XA_AACDEC_CONFIG_FATAL_INVALID_OUTNCHANS            = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_AAC_DEC, 5),
  XA_AACDEC_CONFIG_FATAL_INVALID_SBR_SIGNALING        = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_AAC_DEC, 6),
  XA_AACDEC_CONFIG_FATAL_INVALID_CHANROUTING          = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_AAC_DEC, 7),
  XA_AACDEC_CONFIG_FATAL_INVALID_PCM_WDSZ             = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_AAC_DEC, 8),
  XA_AACDEC_CONFIG_FATAL_INVALID_ZERO_UNUSED_CHANS    = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_AAC_DEC, 9),
  /* Code For Invalid Number of input channels */
  XA_AACDEC_CONFIG_FATAL_INVALID_EXTERNALCHCONFIG    = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_AAC_DEC, 10), // Depricated, no more implemented
  XA_AACDEC_CONFIG_FATAL_INVALID_DECODELAYERS        = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_AAC_DEC, 11), // Depricated, no more implemented
  XA_AACDEC_CONFIG_FATAL_INVALID_EXTERNALBITRATE     = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_AAC_DEC, 12),
  XA_AACDEC_CONFIG_FATAL_INVALID_CONCEALMENT_PARAM   = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_AAC_DEC, 13)
};
/*****************************************************************************/
/* Class 2: Execution Class Errors                                           */
/*****************************************************************************/
/* Nonfatal Errors */
enum xa_error_nonfatal_execute_aac_dec {
  XA_AACDEC_EXECUTE_NONFATAL_INSUFFICIENT_FRAME_DATA            = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_AAC_DEC, 0)
  ,XA_AACDEC_EXECUTE_NONFATAL_RUNTIME_INIT_RAMP_DOWN            = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_AAC_DEC, 1)
  ,XA_AACDEC_EXECUTE_NONFATAL_RAW_FRAME_PARSE_ERROR             = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_AAC_DEC, 2)
  ,XA_AACDEC_EXECUTE_NONFATAL_ADTS_HEADER_ERROR                 = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_AAC_DEC, 3) // Depreciated error, decoder don't return this error anymore
  ,XA_AACDEC_EXECUTE_NONFATAL_ADTS_HEADER_NOT_FOUND             = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_AAC_DEC, 4) // Depreciated error, decoder don't return this error anymore
  ,XA_AACDEC_EXECUTE_NONFATAL_DABPLUS_HEADER_NOT_FOUND          = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_AAC_DEC, 5)
  ,XA_AACDEC_EXECUTE_NONFATAL_LOAS_HEADER_ERROR                 = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_AAC_DEC, 6) // Depreciated error, decoder don't return this error anymore
  ,XA_AACDEC_EXECUTE_NONFATAL_STREAM_CHANGE                     = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_AAC_DEC, 7)
  ,XA_AACDEC_EXECUTE_NONFATAL_HEADER_NOT_FOUND                  = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_AAC_DEC, 8)
  ,XA_AACDEC_EXECUTE_NONFATAL_UNSUPPORTED_FEATURE               = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_AAC_DEC, 9)
  ,XA_AACDEC_EXECUTE_NONFATAL_HEADER_ERROR                      = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_AAC_DEC, 10)
  ,XA_AACDEC_EXECUTE_NONFATAL_PARTIAL_LAST_FRAME                = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_AAC_DEC, 11)
  ,XA_AACDEC_EXECUTE_NONFATAL_EMPTY_INPUT_BUFFER                = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_AAC_DEC, 12)
  ,XA_AACDEC_EXECUTE_NONFATAL_ROUTING_ABSENT_CH_IGNORED         = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_AAC_DEC, 13)
  ,XA_AACDEC_EXECUTE_NONFATAL_NEXT_SYNC_NOT_FOUND               = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_AAC_DEC, 14)
};
/* Fatal Errors */
enum xa_error_fatal_execute_aac_dec {
  XA_AACDEC_EXECUTE_FATAL_PARSING_ERROR                  = XA_ERROR_CODE(xa_severity_fatal, xa_class_execute, XA_CODEC_AAC_DEC, 0) // Depreciated error, decoder don't return this error anymore
  ,XA_AACDEC_EXECUTE_FATAL_RAW_FRAME_PARSE_ERROR          = XA_ERROR_CODE(xa_severity_fatal, xa_class_execute, XA_CODEC_AAC_DEC, 1)
  ,XA_AACDEC_EXECUTE_FATAL_BAD_INPUT_FAILURE              = XA_ERROR_CODE(xa_severity_fatal, xa_class_execute, XA_CODEC_AAC_DEC, 2) // Depreciated error, decoder don't return this error anymore
  ,XA_AACDEC_EXECUTE_FATAL_UNSUPPORTED_FEATURE             = XA_ERROR_CODE(xa_severity_fatal, xa_class_execute, XA_CODEC_AAC_DEC, 3)
  ,XA_AACDEC_EXECUTE_FATAL_ERROR_IN_CHANROUTING           = XA_ERROR_CODE(xa_severity_fatal, xa_class_execute, XA_CODEC_AAC_DEC, 4)
  ,XA_AACDEC_EXECUTE_FATAL_EMPTY_INPUT_BUFFER              = XA_ERROR_CODE(xa_severity_fatal, xa_class_execute, XA_CODEC_AAC_DEC, 5)
  ,XA_AACDEC_EXECUTE_FATAL_LOAS_HEADER_CHANGE            = XA_ERROR_CODE(xa_severity_fatal, xa_class_execute, XA_CODEC_AAC_DEC, 6) // Depreciated error, decoder don't return this error anymore
  ,XA_AACDEC_EXECUTE_FATAL_INIT_ERROR                    = XA_ERROR_CODE(xa_severity_fatal, xa_class_execute, XA_CODEC_AAC_DEC, 7) // Depreciated error, decoder don't return this error anymore
  ,XA_AACDEC_EXECUTE_FATAL_UNKNOWN_STREAM_FORMAT         = XA_ERROR_CODE(xa_severity_fatal, xa_class_execute, XA_CODEC_AAC_DEC, 8)
  ,XA_AACDEC_EXECUTE_FATAL_ADIF_HEADER_NOT_FOUND         = XA_ERROR_CODE(xa_severity_fatal, xa_class_execute, XA_CODEC_AAC_DEC, 9)

};

#include "xa_type_def.h"

/* Relevant for loas build only */
/* PCE status in the bit-stream */
typedef enum {
  XA_AACDEC_PCE_NOT_FOUND_YET = 0,      /* No PCE found in the stream yet. */
  XA_AACDEC_PCE_NEW           = 1,      /* New PCE found in the current frame. */
  XA_AACDEC_PCE_USE_PREV      = 2       /* No PCE in current frame, using previous PCE. */
} xa_aac_dec_pce_status;

/* MetaData Structure */
typedef struct
{
  UWORD8 bMatrixMixdownIdxPresent;   /* Flag indicating if ucMatrixMixdownIndex & bPseudoSurroundEnable were present in PCE */
  UWORD8 ucMatrixMixdownIndex;       /* 2-bit value selecting the coefficient set for matrix downmix.
                                        Note, ucMatrixMixdownIndex is valid only if bMatrixMixdownIdxPresent = 1 */
  UWORD8 bPseudoSurroundEnable;      /* Flag indicating the possibility of mixdown for pseudo surround reproduction.
                                        Note, bPseudoSurroundEnable is valid only if bMatrixMixdownIdxPresent = 1 */
} xa_aac_dec_dwnmix_metadata_t;

/* Structure for downmix levels present in acnillary data (DSE) */
/*
Where
new_dvb_downmix_data:
     Flag for indicating the presence of new downmixing data
     in the current frame.
     0 - no "new" dvb downmixing data
     1 - dvb downmixing data available

mpeg_audio_type:
     2-bits value indicating mpeg audio type.
     0 - Reserved
     1,2 - MPEG1 and MPEG2 Audio data.
     3 - MPEG4 Audio data.
(Refer Section C.4.2.3 and C.5.2.2.1 of ETSI TS 101 154 V1.9.1 document )

dolby_surround_mode:
     2-bits value indicating dolby surround mode.
     0,3 - Reserved
     1 - 2-ch audio is not dolby surround encoded.
     2 - 2-ch audio is dolby surround encoded.
(Refer Section C.4.2.4 and C.5.2.2.2 of ETSI TS 101 154 V1.9.1 document )

center_mix_level_on:
     Flag for the presence of center_mix_level_value.
     0 or 1 are valid values(Refer to ETSI TS 101 154 V1.9.1)
center_mix_level_value;
     3-bit value for the downmix factor for mixing the center channel
     into the stereo output. Values refer to ETSI TS 101 154 V1.9.1
surround_mix_level_on:
     Flag for the presence of surround_mix_level_value
     0 or 1 are valid values(Refer to ETSI TS 101 154 V1.9.1)
surround_mix_level_value:
     3-bit value for the downmix factor for mixing the left and
     right surrond into the stereo output.
     Values refer to ETSI TS 101 154 V1.9.1
(Refer Section C.4.2.10 and C.5.2.4 of ETSI TS 101 154 V1.9.1 document for the above)

coarse_grain_timecode_on;
fine_grain_timecode_on;
     2 bit flags indicating whether the coarse or fine time codes are present or not.
coarse_grain_timecode_value;
fine_grain_timecode_value;
     14 bit values containing the coarse or fine grain_timecode values.
(Refer Section C.4.2.13 / C.4.2.14 and C.5.2.4 of ETSI TS 101 154 V1.9.1 document )

(Details about timecodes:
Resetting of corse_grain_timecode_value based on (coarse_grain_timecode_on == '10') shall NOT
be done by the library. Same shall be true true of fine_grain_time_code_value.

For MPEG4, if the status bit coarse_grain_timecode_status is 0, then both
coarse_grain_timecode_on;
coarse_grain_timecode_value;
shall be set to 0.
Same shall hold true for fine_grain_timecode values.)

*/

typedef struct {
  UWORD8 new_dvb_downmix_data;
  UWORD8 mpeg_audio_type;
  UWORD8 dolby_surround_mode;
  UWORD8 center_mix_level_on;
  UWORD8 center_mix_level_value;
  UWORD8 surround_mix_level_on;
  UWORD8 surround_mix_level_value;
  UWORD8 coarse_grain_timecode_on;
  UWORD coarse_grain_timecode_value;
  UWORD8 fine_grain_timecode_on;
  UWORD fine_grain_timecode_value;
} xa_aac_dec_dwnmix_level_dvb_info_t;

#define MAX_NUM_CHANNELS   8
#define MAX_NUM_DRC_BANDS    16
/*
  drc_info_valid: Flag to indicate if the rest of the nine elements in 
                structure are valid (1) or not (0) for current frame.
  The definition and values of the rest of the nine elements are 
                the same as defined in Table 4.52 of ISO/IEC 14496-3.
*/
typedef struct {
  unsigned char drc_info_valid;
  unsigned char exclude_masks[MAX_NUM_CHANNELS];
  unsigned char drc_bands_present;
  unsigned char drc_interpolation_scheme;
  unsigned char drc_num_bands;
  unsigned char drc_band_incr;
  unsigned char drc_band_top[MAX_NUM_DRC_BANDS];
  unsigned char prog_ref_level_present;
  unsigned char prog_ref_level;
  char dyn_rng_dbx4[MAX_NUM_DRC_BANDS];
} xa_aac_dec_parsed_drc_info_t;

typedef struct _xa_aac_dec_parsed_pce_info_t
{
    /* Number of elements */
    UWORD8  num_front_channel_elements;
    UWORD8  num_side_channel_elements;
    UWORD8  num_back_channel_elements;
    UWORD8  num_lfe_channel_elements;

    /* channels per element */
    UWORD8  num_front_channels_per_element[16];
    UWORD8  num_side_channels_per_element[16];
    UWORD8  num_back_channels_per_element[16];
    UWORD8  num_lfe_channels_per_element[16];

    /* channel height info */
    UWORD8  front_element_height_info[16];
    UWORD8  side_element_height_info[16];
    UWORD8  back_element_height_info[16];
}xa_aac_dec_parsed_pce_info_t;

#if defined(USE_DLL) && defined(_WIN32)
#define DLL_SHARED __declspec(dllimport)
#elif defined (_WINDLL)
#define DLL_SHARED __declspec(dllexport)
#else
#define DLL_SHARED
#endif

#if defined(__cplusplus)
extern "C" {
#endif  /* __cplusplus */
DLL_SHARED xa_codec_func_t xa_aac_dec;
DLL_SHARED xa_codec_func_t xa_dabplus_dec;
#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif /* __XA_AAC_DEC_API_H__ */
