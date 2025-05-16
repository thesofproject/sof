/*  Copyright 2004-2025 NXP
 *
 * SPDX-License-Identifier: MIT
 * This license applies ONLY to this header file EAP16.h
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
 *
 */

/*****************************************************************************************

     $Author: beq03888 $
     $Revision: 16643 $
     $Date: 2011-10-14 09:00:36 +0200 (Fri, 14 Oct 2011) $

*****************************************************************************************/

/****************************************************************************************/
/*                                                                                      */
/*  Header file for the application layer interface of Concert Sound, Bass Enhancement  */
/*  and volume management bundle.                                                       */
/*                                                                                      */
/*  This files includes all definitions, types, structures and function                 */
/*  prototypes required by the calling layer. All other types, structures and           */
/*  functions are private.                                                              */
/*                                                                                      */
/****************************************************************************************/
/*                                                                                      */
/*  Note: 1                                                                             */
/*  =======                                                                             */
/*  The algorithm can execute either with separate input and output buffers or with     */
/*  a common buffer, i.e. the data is processed in-place.                               */
/*                                                                                      */
/****************************************************************************************/
/*                                                                                      */
/*  Note: 2                                                                             */
/*  =======                                                                             */
/*  Three data formats are support Stereo and Mono. The input data is					*/
/*  interleaved as follows:                             -----                           */
/*                                                                                      */
/*  Byte Offset         Stereo Input                Mono Input							*/
/*  ===========         ============         ====================						*/
/*      0               Left Sample #1          Mono Sample #1							*/
/*      2               Right Sample #1         Mono Sample #1							*/
/*      4               Left Sample #2          Mono Sample #2							*/
/*      6               Right Sample #2         Mono Sample #2							*/
/*      .                      .                     .                                  */
/*      .                      .                     .                                  */
/*																						*/		
/*	For output buffer there is 3 cases :												*/
/*		------																			*/
/*		1) CROSSOVER is DISABLE															*/
/*	Byte Offset       Stereo Input/Output       Mono Input/Output						*/
/*  ===========       ===================       =================						*/
/*      0               Left Sample #1            Mono Sample #1						*/
/*      2               Right Sample #1           Mono Sample #1						*/
/*      4               Left Sample #2            Mono Sample #2						*/
/*      6               Right Sample #2           Mono Sample #2						*/
/*      .                      .                     .                                  */
/*      .                      .                     .                                  */
/*																						*/
/*      2) CROSSOVER is ENABLE & input/output in STEREO									*/
/*          pOutData[0] will be the output Low band and pOutData[1] the High band       */	
/*  Stereo Input		   pOutData[0] in stereo       pOutData[1] in stereo 			*/
/*  ===================    =====================       =====================			*/
/*  Left Sample  #1          Left Sample LB  #1           Left Sample HB  #1			*/
/*  Right Sample #1          Right Sample LB #1           Right Sample HB #1			*/
/*  Left Sample  #2          Left Sample LB  #2           Left Sample HB  #2			*/
/*  Right Sample #2          Right Sample LB #2           Right Sample HB #2			*/
/*      .                            .                            .                     */
/*      .                            .                            .                     */
/*																						*/
/*      3) CROSSOVER is ENABLE & input/output in MONO									*/
/*          pOutData[0] will be the output Low band and pOutData[1] the High band       */
/*  Mono Input		       pOutData[0] in mono         pOutData[1] in mono 			    */
/*  ===============        =====================       =====================			*/
/*  MONO Sample  #1          MONO Sample LB  #1           MONO Sample HB  #1			*/
/*  MONO Sample  #2          MONO Sample LB  #2           MONO Sample HB  #2			*/
/*  MONO Sample  #3          MONO Sample LB  #3           MONO Sample HB  #3			*/
/*  MONO Sample  #4          MONO Sample LB  #4           MONO Sample HB  #4			*/
/*      .                            .                            .                     */
/*      .                            .                            .                     */
/*																						*/
/****************************************************************************************/

#ifndef __LVM_H__
#define __LVM_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#ifdef  ALGORITHM_CS
#define ALGORITHM_VIRTUALIZER
#endif  /* ALGORITHM_CS */

#ifdef ALGORITHM_DBE
#define ALGORITHM_BASS
#else
#ifdef ALGORITHM_PB
#define ALGORITHM_BASS
#endif   /* ALGORITHM_PB */
#endif   /* ALGORITHM_DBE */


/****************************************************************************************/
/*                                                                                      */
/*  Includes                                                                            */
/*                                                                                      */
/****************************************************************************************/

#include "LVC_Types.h"


/****************************************************************************************/
/*                                                                                      */
/*  Definitions                                                                         */
/*                                                                                      */
/****************************************************************************************/

/* MAXIMAL VALUE LIMIT */
#define LVM_MAX_NUM_CHANNELS                  2     /* Maximum number of interleaved input channels */
#define MAX_INTERNAL_BLOCKSIZE             1024     /* Maximum internal block size authorized (multiple of 64)*/
#define LVM_HEADROOM_MAX_NBANDS               5     /* Headroom management */
#define LVM_EQNB_MAX_BANDS_NBR               10     /* EQNB Maximal band number */
#define LVM_PSA_MAX_NUMBANDS                 64     /* Maximum Number of PSA Bands*/

/* Memory table*/
#define LVM_NR_MEMORY_REGIONS                 4     /* Number of memory regions */

/* Concert Sound effect level presets */
#ifdef ALGORITHM_VIRTUALIZER 
#define LVM_CS_EFFECT_NONE                    0     /* 0% effect, minimum value */
#define LVM_CS_EFFECT_LOW                 16384     /* 50% effect */
#define LVM_CS_EFFECT_MED                 24576     /* 75% effect */
#define LVM_CS_EFFECT_HIGH                32767     /* 100% effect, maximum value */
#endif /*end  ALGORITHM_VIRTUALIZER */

#ifdef ALGORITHM_TE
#define LVM_TE_LOW_MIPS                   32767     /* Treble enhancement 6dB Mips saving mode */
#endif /* end ALGORITHM_TE */

/* Bass enhancement effect level presets */
#ifdef ALGORITHM_BASS
#define LVM_BE_0DB                            0     /* 0dB boost, no effect */
#define LVM_BE_3DB                            3     /* +3dB boost */
#define LVM_BE_6DB                            6     /* +6dB boost */
#define LVM_BE_9DB                            9     /* +9dB boost */
#define LVM_BE_12DB                          12     /* +12dB boost */
#define LVM_BE_15DB                          15     /* +15dB boost */
#endif /* end ALGORITHM_BASS */

/****************************************************************************************/
/*                                                                                      */
/*  Types                                                                               */
/*                                                                                      */
/****************************************************************************************/

/* Instance handle */
typedef void *LVM_Handle_t;


/* Status return values */
typedef enum
{
    LVM_SUCCESS            = 0,                     /* Successful return from a routine */
    LVM_ALIGNMENTERROR     = 1,                     /* Memory alignment error */
    LVM_NULLADDRESS        = 2,                     /* NULL allocation address */
    LVM_INVALIDNUMSAMPLES  = 3,                     /* Invalid number of samples */
    LVM_WRONGAUDIOTIME     = 4,                     /* Wrong time value for audio time*/
    LVM_ALGORITHMDISABLED  = 5,                     /* Algorithm is disabled*/
    LVM_NOT_INITIALIZED    = 6,                     /* Process function was called for a non-initialized module */
	LVM_INVALIDNXPPLATFORM = 7,                     /* Invalid NXP platform */
	// OUT OF RANGE HANDLE Must stay at the end of the enum
	LVM_OUTOFRANGE         = 8,                     /* Out of range control parameter (without details) */
	/* OUT OF RANGE WITH DETAILS */
	LVM_OUTOFRANGE_GENERAL_PARAMS			= 9,
	LVM_OUTOFRANGE_SPEAKER_TYPES			= 10,
	LVM_OUTOFRANGE_VIRTUALIZER_OM			= 11,
	LVM_OUTOFRANGE_VIRTUALIZER_TYPE         = 12,
	LVM_OUTOFRANGE_VIRTUALIZER_REVERB       = 13,
	LVM_OUTOFRANGE_CS_EFFECT				= 14,
	LVM_OUTOFRANGE_USER_EQNB				= 15,
	LVM_OUTOFRANGE_USER_EQNB_BAND_DEF       = 16,
	LVM_OUTOFRANGE_PRODUCT_EQNB				= 17,
	LVM_OUTOFRANGE_PRODUCT_EQNB_BAND_DEF    = 18,
	LVM_OUTOFRANGE_BE						= 19,
	LVM_OUTOFRANGE_PB						= 20,
	LVM_OUTOFRANGE_VC_LEVEL					= 21,
	LVM_OUTOFRANGE_VC_BALANCE				= 22,
	LVM_OUTOFRANGE_TE						= 23,
	LVM_OUTOFRANGE_LM						= 24,
	LVM_OUTOFRANGE_LM_SPEAKER_CUTOFF        = 25,
	LVM_OUTOFRANGE_AVL						= 26,
	LVM_OUTOFRANGE_TG_OM					= 27,
	LVM_OUTOFRANGE_TG						= 28,
	LVM_OUTOFRANGE_PSA_RATE					= 29,
	LVM_OUTOFRANGE_PSA_ENABLE				= 30,
	LVM_OUTOFRANGE_PSA_NUMBAND				= 31,
	LVM_OUTOFRANGE_LIMP_OM					= 32,
	LVM_OUTOFRANGE_LIMP_THRESHOLD			= 33,
	LVM_OUTOFRANGE_LIMR_OM					= 34,
	LVM_OUTOFRANGE_LIMR_THRESHOLD			= 35,
	LVM_OUTOFRANGE_LIMR_REFERENCE			= 36,
	LVM_OUTOFRANGE_CS_AP_MODE				= 37,
	LVM_OUTOFRANGE_CS_AP					= 38,
	LVM_OUTOFRANGE_XO_OPERATINGMODE			= 39,                
	LVM_OUTOFRANGE_XO_CUTOFFFREQUENCY		= 40,               
	

	LVM_RETURNSTATUS_DUMMY = LVM_MAXENUM
} LVM_ReturnStatus_en;


/* Buffer Management mode */
typedef enum
{
    LVM_MANAGED_BUFFERS   = 0,
    LVM_UNMANAGED_BUFFERS = 1,
    LVM_BUFFERS_DUMMY     = LVM_MAXENUM
} LVM_BufferMode_en;



/* Output device type */
typedef enum
{
    LVM_HEADPHONES             = 0,
    LVM_MOBILE_SPEAKERS_SMALL  = 2,
    LVM_MOBILE_SPEAKERS_MEDIUM = 3,
    LVM_MOBILE_SPEAKERS_LARGE  = 4,
    LVM_SPEAKERTYPE_MAX        = LVM_MAXENUM
} LVM_OutputDeviceType_en;

typedef enum
{
	LVM_IMXRT1050 = 1,						   // I.MXRT1050 : EAP running on Cortex-M7
	LVM_IMXRT1060 = 2,                         // I.MXRT1060 : EAP running on Cortex-M7
	LVM_IMXRT1064 = 3,						   // I.MXRT1064 : EAP running on Cortex-M7
	LVM_IMXRT1170 = 4,                         // I.MXRT1170 : EAP running on Cortex-M7
	LVM_LPC55     = 5,						   // LPC55	     : EAP running on Cortex-M33
	LVM_IMXRT500  = 6,                         // I.MXRT500  : EAP running on FusionF1
	LVM_IMXRT600  = 7,                         // I.MXRT600  : EAP running on HIFI4
	LVM_MAX_PLATFORM = LVM_MAXENUM,
}EAP_NXPPlatform_en;

/* Virtualizer mode selection*/
#ifdef ALGORITHM_VIRTUALIZER
typedef enum
{
    LVM_CONCERTSOUND       = 0,
    LVM_VIRTUALIZERTYPE_DUMMY   = LVM_MAXENUM
} LVM_VirtualizerType_en;
#endif  /* ALGORITHM_VIRTUALIZER */

/* N-Band Equaliser operating mode */
#if (ALGORITHM_EQNB) || (ALGORITHM_PR_EQNB)
typedef enum
{
    LVM_EQNB_OFF   = 0,
    LVM_EQNB_ON    = 1,
    LVM_EQNB_DUMMY = LVM_MAXENUM
} LVM_EQNB_Mode_en;
#endif /* ALGORITHM_EQNB  || ALGORITHM_PR_EQNB*/

/* Filter mode control */
typedef enum
{
    LVM_EQNB_FILTER_OFF = 0,
    LVM_EQNB_FILTER_ON  = 1,
    LVM_EQNB_FILTER_DUMMY = LVM_MAXENUM
} LVM_EQNB_FilterMode_en;

/* Bass Enhancement operating mode */
#ifdef ALGORITHM_BASS
typedef enum
{
    LVM_BE_OFF   = 0,
    LVM_BE_ON    = 1,
    LVM_BE_DUMMY = LVM_MAXENUM
} LVM_BE_Mode_en;

/* Bass Enhancement centre frequency selection control */
typedef enum
{
    LVM_BE_CENTRE_55Hz  = 0,
    LVM_BE_CENTRE_66Hz  = 1,
    LVM_BE_CENTRE_78Hz  = 2,
    LVM_BE_CENTRE_90Hz  = 3,
    LVM_BE_CENTRE_DUMMY = LVM_MAXENUM
} LVM_BE_CentreFreq_en;

/* Bass Enhancement HPF selection control */
typedef enum
{
    LVM_BE_HPF_OFF   = 0,
    LVM_BE_HPF_ON    = 1,
    LVM_BE_HPF_DUMMY = LVM_MAXENUM
} LVM_BE_FilterSelect_en;


#endif /* ALGORITHM_BASS */

/* Volume Control operating mode */
typedef enum
{
    LVM_VC_OFF   = 0,
    LVM_VC_ON    = 1,
    LVM_VC_DUMMY = LVM_MAXENUM
} LVM_VC_Mode_en;

/* Treble Enhancement operating mode */
#ifdef ALGORITHM_TE
typedef enum
{
    LVM_TE_OFF   = 0,
    LVM_TE_ON    = 1,
    LVM_TE_DUMMY = LVM_MAXENUM
} LVM_TE_Mode_en;
#endif /* ALGORITHM_TE */

/* Loudness Maximiser operating mode */
#ifdef ALGORITHM_LM
typedef enum
{
    LVM_LM_OFF   = 0,
    LVM_LM_ON    = 1,
    LVM_LM_DUMMY = LVM_MAXENUM
}LVM_LM_Mode_en;

/* Loudness Maximiser effect setting */
typedef enum
{
    LVM_LM_GENTLE       = 0,
    LVM_LM_MEDIUM       = 1,
    LVM_LM_EXTREME      = 2,
    LVM_LM_EFFECT_DUMMY = LVM_MAXENUM
}LVM_LM_Effect_en;
#endif /* ALGORITHM_LM */

/* AVL operating mode */
#ifdef ALGORITHM_AVL
typedef enum
{
    LVM_AVL_OFF   = 0,
    LVM_AVL_ON    = 1,
    LVM_AVL_DUMMY = LVM_MAXENUM
} LVM_AVL_Mode_en;
#endif /* ALGORITHM_AVL */

/* Headroom management operating mode */
typedef enum
{
    LVM_HEADROOM_OFF   = 0,
    LVM_HEADROOM_ON    = 1,
    LVM_Headroom_DUMMY = LVM_MAXENUM
} LVM_Headroom_Mode_en;

/* Tone Generator operating mode */
#ifdef ALGORITHM_TG
typedef enum
{
    LVM_TG_OFF        = 0,
    LVM_TG_CONTINUOUS = 1,
    LVM_TG_ONESHOT    = 2,
    LVM_TG_DUMMY      = LVM_MAXENUM
} LVM_TG_Mode_en;

/* Tone Generator sweep mode */
typedef enum
{
    LVM_TG_SWEEPLIN    = 0,
    LVM_TG_SWEEPLOG    = 1,
    LVM_TG_SWEEP_DUMMY = LVM_MAXENUM
} LVM_TG_SweepMode_en;
#endif /* ALGORITHM_TG */

#ifdef ALGORITHM_XO
typedef enum
{
	LVM_XO_MODE_OFF = 0,
	LVM_XO_MODE_ON = 1
}LVM_XO_MODE_en;
#endif /*ALGORITHM_XO*/


#ifdef ALGORITHM_PSA
typedef enum
{
    LVM_PSA_SPEED_SLOW,                                  /* Peak decaying at slow speed */
    LVM_PSA_SPEED_MEDIUM,                                /* Peak decaying at medium speed */
    LVM_PSA_SPEED_FAST,                                  /* Peak decaying at fast speed */
    LVM_PSA_SPEED_DUMMY = LVM_MAXENUM
} LVM_PSA_DecaySpeed_en;

typedef enum
{
    LVM_PSA_OFF   = 0,
    LVM_PSA_ON    = 1,
    LVM_PSA_DUMMY = LVM_MAXENUM
} LVM_PSA_Mode_en;
#endif /* ALGORITHM_PSA */

#ifdef ALGORITHM_LIMP
typedef enum
{
    LVM_LIMP_OFF   = 0,
    LVM_LIMP_ON    = 1,
    LVM_LIMP_DUMMY = LVM_MAXENUM
} LVM_LIMP_Mode_en;
#endif /* ALGORITHM_LIMP */

#ifdef ALGORITHM_LIMR
typedef enum
{
    LVM_LIMR_OFF   = 0,
    LVM_LIMR_ON    = 1,
    LVM_LIMR_DUMMY = LVM_MAXENUM
} LVM_LIMR_Mode_en;

typedef enum
{
    LVM_LIMR_REF_INPUT  = 0,
    LVM_LIMR_REF_0DBFS  = 1,
    LVM_LIMR_REF_DUMMY  = LVM_MAXENUM
} LVM_LIMR_Reference_en;
#endif /* ALGORITHM_LIMR */

// Adavanced parameter mode
typedef enum
{
    LVM_AP_DEFAULT  = 0,
    LVM_AP_MANUAL   = 1,
    LVM_AP_DUMMY  = LVM_MAXENUM
} LVM_AP_MODE_en;

/****************************************************************************************/
/*                                                                                      */
/*  Structures                                                                          */
/*                                                                                      */
/****************************************************************************************/

/* Version information */
typedef struct
{
    LVM_CHAR                    *pVersionNumber;        /* Pointer to the version number in the format X.YY.ZZ */
    LVM_CHAR                    *pPlatform;             /* Pointer to the library platform type */
} LVM_VersionInfo_st;

/* Memory table containing the region definitions */
typedef struct
{
    LVM_MemoryRegion_st         Region[LVM_NR_MEMORY_REGIONS];  /* One definition for each region */
} LVM_MemTab_t;


/* N-Band equaliser band definition */
typedef struct
{
    LVM_INT16                   Gain;                   /* Band gain in dB */
    LVM_UINT16                  Frequency;              /* Band centre frequency in Hz */
    LVM_UINT16                  QFactor;                /* Band quality factor (x100) */
} LVM_EQNB_BandDef_t;

/* Headroom band definition */
typedef struct
{
    LVM_UINT16                  Limit_Low;              /* Low frequency limit of the band in Hertz */
    LVM_UINT16                  Limit_High;             /* High frequency limit of the band in Hertz */
    LVM_INT16                   Headroom_Offset;        /* Headroom = biggest band gain - Headroom_Offset */
} LVM_HeadroomBandDef_t;

/* Control Parameter structure */
typedef struct
{
    /* General parameters */
    LVM_Mode_en                 OperatingMode;          /* Bundle operating mode On/Bypass */
    LVM_Fs_en                   SampleRate;             /* Sample rate */
    LVM_Format_en               SourceFormat;           /* Input data format */
    LVM_OutputDeviceType_en     SpeakerType;            /* Output device type */
    LVM_SpeakerType_en          SpeakerTypeInternal;    /* Device speaker type, mono or stereo */

#ifdef ALGORITHM_CS
    /* Concert Sound Virtualizer parameters*/
    LVM_Mode_en                 VirtualizerOperatingMode; /* Virtualizer operating mode On/Off */
    LVM_VirtualizerType_en      VirtualizerType;          /* Virtualizer type: ConcertSound, CinemaSound Music or CinemaSound Movie */
    LVM_UINT16                  VirtualizerReverbLevel;   /* Virtualizer reverb level in % */
    LVM_INT16                   CS_EffectLevel;           /* Concert Sound effect level */
#endif /* ALGORITHM_CS */

#ifdef ALGORITHM_EQNB
    /* N-Band Equaliser parameters */
    LVM_EQNB_Mode_en            EQNB_OperatingMode;     /* N-Band Equaliser operating mode */
    LVM_EQNB_FilterMode_en      EQNB_LPF_Mode;          /* Low pass filter */
    LVM_INT16                   EQNB_LPF_CornerFreq;
    LVM_EQNB_FilterMode_en      EQNB_HPF_Mode;          /* High pass filter */
    LVM_INT16                   EQNB_HPF_CornerFreq;
    LVM_UINT16                  EQNB_NBands;            /* Number of bands */
    LVM_EQNB_BandDef_t          *pEQNB_BandDefinition;  /* Pointer to equaliser definitions */
#endif /* ALGORITHM_EQNB */

#ifdef ALGORITHM_PR_EQNB
    /* N-Band Equaliser parameters */
    LVM_EQNB_Mode_en            PR_EQNB_OperatingMode;     /* N-Band Equaliser operating mode */
    LVM_EQNB_FilterMode_en      PR_EQNB_LPF_Mode;          /* Low pass filter */
    LVM_INT16                   PR_EQNB_LPF_CornerFreq;
	LVM_EQNB_FilterMode_en      PR_EQNB_HPF_Mode;          /* High pass filter */
    LVM_INT16                   PR_EQNB_HPF_CornerFreq;
    LVM_UINT16                  PR_EQNB_NBands;            /* Number of bands */
    LVM_EQNB_BandDef_t          *pPR_EQNB_BandDefinition;  /* Pointer to equaliser definitions */
#endif /* ALGORITHM_PR_EQNB */

#ifdef ALGORITHM_DBE
    /* Bass Enhancement parameters */
    LVM_BE_Mode_en              BE_OperatingMode;       /* Bass Enhancement operating mode */
    LVM_INT16                   BE_EffectLevel;         /* Bass Enhancement effect level */
    LVM_BE_CentreFreq_en        BE_CentreFreq;          /* Bass Enhancement centre frequency */
    LVM_BE_FilterSelect_en      BE_HPF;                 /* Bass Enhancement high pass filter selector */

#endif /* ALGORITHM_DBE */
#ifdef ALGORITHM_PB
    /* Bass Enhancement parameters */
    LVM_BE_Mode_en              BE_OperatingMode;       /* Bass Enhancement operating mode */
    LVM_INT16                   BE_EffectLevel;         /* Bass Enhancement effect level */
    LVM_BE_CentreFreq_en        BE_CentreFreq;          /* Bass Enhancement centre frequency */
    LVM_BE_FilterSelect_en      BE_HPF;                 /* Bass Enhancement high pass filter selector */

#endif /* ALGORITHM_PB */
    /* Volume Control parameters */
    LVM_INT16                   VC_EffectLevel;         /* Volume Control setting in dBs */
    LVM_INT16                   VC_Balance;             /* Left Right Balance control in dB (-96 to 96 dB), -ve values reduce */

#ifdef ALGORITHM_TE
    /* Treble Enhancement parameters */
    LVM_TE_Mode_en              TE_OperatingMode;       /* Treble Enhancement On/Off */
    LVM_INT16                   TE_EffectLevel;         /* Treble Enhancement gain dBs */

#endif /* ALGORITHM_TE */
#ifdef ALGORITHM_LM
    /* Loudness Maximiser parameters */
    LVM_LM_Mode_en              LM_OperatingMode;       /* Loudness Maximiser operating mode */
    LVM_LM_Effect_en            LM_EffectLevel;         /* Loudness Maximiser effect level */
    LVM_UINT16                  LM_Attenuation;         /* Loudness Maximiser output attenuation */
    LVM_UINT16                  LM_CompressorGain;      /* Loudness Maximiser output compressor gain */
    LVM_UINT16                  LM_SpeakerCutOff;       /* Loudness Maximiser speaker cut off frequency */

#endif /* ALGORITHM_LM */

#ifdef ALGORITHM_AVL
    /* AVL parameters */
    LVM_AVL_Mode_en             AVL_OperatingMode;      /* AVL operating mode */

#endif /* ALGORITHM_AVL */

#ifdef ALGORITHM_TG
    /* Tone Generator parameters */
    LVM_TG_Mode_en              TG_OperatingMode;       /* Tone generator mode */
    LVM_TG_SweepMode_en         TG_SweepMode;           /* Log or linear sweep */
    LVM_UINT16                  TG_StartFrequency;      /* Sweep start frequency in Hz */
    LVM_INT16                   TG_StartAmplitude;      /* Sweep start amplitude in dBr */
    LVM_UINT16                  TG_StopFrequency;       /* Sweep stop frequency in Hz */
    LVM_INT16                   TG_StopAmplitude;       /* Sweep stop amplitude in dBr */
    LVM_UINT16                  TG_SweepDuration;       /* Sweep duration in seconds, 0 for infinite duration tone */
    LVM_Callback                pTG_CallBack;           /* End of sweep callback */
    LVM_INT16                   TG_CallBackID;          /* Callback ID*/
    void                        *pTGAppMemSpace;        /* Application instance handle or memory area */
#endif /* ALGORITHM_TG */

#ifdef ALGORITHM_PSA
    /* General Control */
    LVM_PSA_Mode_en             PSA_Enable;

    /* Spectrum Analyzer parameters */
    LVM_PSA_DecaySpeed_en       PSA_PeakDecayRate;      /* Peak value decay rate*/
    LVM_UINT16                  PSA_NumBands;           /* Number of Bands*/
#endif /* ALGORITHM_PSA */

#ifdef ALGORITHM_LIMP
	LVM_LIMP_Mode_en			LIMP_OperatingMode;		/* LIMP operating mode */
	LVM_INT16					LIMP_Threshold;		    /* LIMP threshold in dB */
#endif /* ALGORITHM_LIMP */

#ifdef ALGORITHM_LIMR
	LVM_LIMR_Mode_en			LIMR_OperatingMode;		/* LIMR operating mode */
	LVM_LIMR_Reference_en		LIMR_Reference;	 		/* LIMR reference input */	
	LVM_INT16					LIMR_Threshold;		    /* LIMR threshold in dB */
#endif /* ALGORITHM_LIMR */

#ifdef ALGORITHM_CS
    LVM_AP_MODE_en				CS_AP_Mode;				  /* concert sound advanced paramameter mode */
	LVM_INT16                   CS_AP_MidGain;            /* MidChannelGain */
    LVM_UINT16                  CS_AP_MidCornerFreq;      /* Shelving Filter Corner Frequency */
    LVM_UINT16                  CS_AP_SideHighPassCutoff; /* SideBoost HighPassFilter Corner Frequency */
    LVM_UINT16                  CS_AP_SideLowPassCutoff;  /* SideBoost LowPassFilter Corner Frequency */
    LVM_INT16                   CS_AP_SideGain;           /* Side Channel Gain */
#endif

#ifdef ALGORITHM_XO
	LVM_Mode_en					XO_OperatingMode;		  /* Crossover operating mode*/
	LVM_UINT16					XO_cutoffFrequency;		  /* Crossover cut-off frequency*/

#endif/*ALGORITHM_XO*/
} LVM_ControlParams_t;


/* Instance Parameter structure */
typedef struct
{
    /* General */
    LVM_BufferMode_en           BufferMode;             /* Buffer management mode */
    LVM_UINT16                  MaxBlockSize;           /* Maximum processing block size */

    /* N-Band Equaliser */
    LVM_UINT16                  EQNB_NumBands;          /* Maximum number of User equaliser bands */
	LVM_UINT16                  PR_EQNB_NumBands;       /* Maximum number of Product equaliser bands */
	EAP_NXPPlatform_en			Platform;				/* NXP Platform where EAP is playing on (LVM_IMXRT1050,LVM_IMXRT1060, LVM_IMXRT1064, LVM_IMXRT1170, LVM_LPC55, LVM_IMXRT500, LVM_IMXRT600)*/
	

#ifdef ALGORITHM_PSA
    /* PSA */
    LVM_UINT16                  PSA_HistorySize;         /* PSA History size in ms: 200 to 5000 */
    LVM_UINT16                  PSA_MaxBands;            /* Maximum number of bands: 6 to 64 */
    LVM_UINT16                  PSA_SpectrumUpdateRate;  /* Spectrum update rate : 10 to 25*/
    LVM_PSA_Mode_en             PSA_Included;            /* Controls the instance memory allocation for PSA: ON/OFF */
#endif /* ALGORITHM_PSA */
} LVM_InstParams_t;


/* Headroom management parameter structure */
typedef struct
{
    LVM_Headroom_Mode_en        Headroom_OperatingMode; /* Headroom Control On/Off */
    LVM_HeadroomBandDef_t       *pHeadroomDefinition;   /* Pointer to headroom bands definition */
    LVM_UINT16                  NHeadroomBands;         /* Number of headroom bands */
} LVM_HeadroomParams_t;


/****************************************************************************************/
/*                                                                                      */
/*  Function Prototypes                                                                 */
/*                                                                                      */
/****************************************************************************************/


/****************************************************************************************/
/*                                                                                      */
/* FUNCTION:                LVM_GetVersionInfo                                          */
/*                                                                                      */
/* DESCRIPTION:                                                                         */
/*  This function is used to retrieve information about the library's version.          */
/*                                                                                      */
/* PARAMETERS:                                                                          */
/*  pVersion                Pointer to an empty version info structure                  */
/*                                                                                      */
/* RETURNS:                                                                             */
/*  LVM_SUCCESS             Succeeded                                                   */
/*  LVM_NULLADDRESS         when pVersion is NULL                                       */
/*                                                                                      */
/* NOTES:                                                                               */
/*  1.  This function may be interrupted by the LVM_Process function                    */
/*                                                                                      */
/****************************************************************************************/
#ifdef __DLL_EXPORT
__declspec(dllexport)
#endif /* __DLL_EXPORT */
LVM_ReturnStatus_en LVM_GetVersionInfo(LVM_VersionInfo_st  *pVersion);


/****************************************************************************************/
/*                                                                                      */
/* FUNCTION:                LVM_GetMemoryTable                                          */
/*                                                                                      */
/* DESCRIPTION:                                                                         */
/*  This function is used for memory allocation and free. It can be called in           */
/*  two ways:                                                                           */
/*                                                                                      */
/*      hInstance = NULL                Returns the memory requirements                 */
/*      hInstance = Instance handle     Returns the memory requirements and             */
/*                                      allocated base addresses for the instance       */
/*                                                                                      */
/*  When this function is called for memory allocation (hInstance=NULL) the memory      */
/*  base address pointers are NULL on return.                                           */
/*                                                                                      */
/*  When the function is called for free (hInstance = Instance Handle) the memory       */
/*  table returns the allocated memory and base addresses used during initialisation.   */
/*                                                                                      */
/* PARAMETERS:                                                                          */
/*  hInstance               Instance Handle                                             */
/*  pMemoryTable            Pointer to an empty memory definition table                 */
/*  pInstParams             Pointer to the instance parameters                          */
/*                                                                                      */
/* RETURNS:                                                                             */
/*  LVM_SUCCESS             Succeeded                                                   */
/*  LVM_NULLADDRESS         When one of pMemoryTable or pInstParams is NULL             */
/*  LVM_OUTOFRANGE          When any of the Instance parameters are out of range        */
/*                                                                                      */
/* NOTES:                                                                               */
/*  1.  This function may be interrupted by the LVM_Process function                    */
/*                                                                                      */
/****************************************************************************************/
#ifdef __DLL_EXPORT
__declspec(dllexport)
#endif /* __DLL_EXPORT */
LVM_ReturnStatus_en LVM_GetMemoryTable(LVM_Handle_t         hInstance,
                                       LVM_MemTab_t         *pMemoryTable,
                                       LVM_InstParams_t     *pInstParams);


/****************************************************************************************/
/*                                                                                      */
/* FUNCTION:                LVM_GetInstanceHandle                                       */
/*                                                                                      */
/* DESCRIPTION:                                                                         */
/*  This function is used to create a bundle instance. It returns the created instance  */
/*  handle through phInstance. All parameters are set to their default, inactive state. */
/*                                                                                      */
/* PARAMETERS:                                                                          */
/*  phInstance              pointer to the instance handle                              */
/*  pMemoryTable            Pointer to the memory definition table                      */
/*  pInstParams             Pointer to the instance parameters                          */
/*                                                                                      */
/* RETURNS:                                                                             */
/*  LVM_SUCCESS             Initialisation succeeded                                    */
/*  LVM_ALIGNMENTERROR      Instance or scratch memory on incorrect alignment           */
/*  LVM_NULLADDRESS         Instance or scratch memory has a NULL pointer               */
/*                                                                                      */
/* NOTES:                                                                               */
/*  1. This function must not be interrupted by the LVM_Process function                */
/*                                                                                      */
/****************************************************************************************/
#ifdef __DLL_EXPORT
__declspec(dllexport)
#endif /* __DLL_EXPORT */
LVM_ReturnStatus_en LVM_GetInstanceHandle(LVM_Handle_t        *phInstance,
                                          LVM_MemTab_t        *pMemoryTable,
                                          LVM_InstParams_t    *pInstParams);


/****************************************************************************************/
/*                                                                                      */
/* FUNCTION:                LVM_ClearAudioBuffers                                       */
/*                                                                                      */
/* DESCRIPTION:                                                                         */
/*  This function is used to clear the internal audio buffers of the bundle.            */
/*                                                                                      */
/* PARAMETERS:                                                                          */
/*  hInstance               Instance handle                                             */
/*                                                                                      */
/* RETURNS:                                                                             */
/*  LVM_SUCCESS             Buffers Cleared                                             */
/*  LVM_NULLADDRESS         Instance is NULL                                            */
/*                                                                                      */
/* NOTES:                                                                               */
/*  1. This function may be interrupted by the LVM_Process function                */
/*                                                                                      */
/****************************************************************************************/
#ifdef __DLL_EXPORT
__declspec(dllexport)
#endif /* __DLL_EXPORT */
LVM_ReturnStatus_en LVM_ClearAudioBuffers(LVM_Handle_t  hInstance);


/****************************************************************************************/
/*                                                                                      */
/* FUNCTION:                 LVM_GetControlParameters                                   */
/*                                                                                      */
/* DESCRIPTION:                                                                         */
/*  Request the LifeVibes module parameters. The current parameter set is returned      */
/*  via the parameter pointer.                                                          */
/*                                                                                      */
/* PARAMETERS:                                                                          */
/*  hInstance                Instance handle                                            */
/*  pParams                  Pointer to an empty parameter structure                    */
/*                                                                                      */
/* RETURNS:                                                                             */
/*  LVM_SUCCESS          Succeeded                                                      */
/*  LVM_NULLADDRESS      when any of hInstance or pParams is NULL                       */
/*                                                                                      */
/* NOTES:                                                                               */
/*  1.  This function may be interrupted by the LVM_Process function                    */
/*                                                                                      */
/****************************************************************************************/
#ifdef __DLL_EXPORT
__declspec(dllexport)
#endif /* __DLL_EXPORT */
LVM_ReturnStatus_en LVM_GetControlParameters(LVM_Handle_t           hInstance,
                                             LVM_ControlParams_t    *pParams);


/****************************************************************************************/
/*                                                                                      */
/* FUNCTION:                LVM_SetControlParameters                                    */
/*                                                                                      */
/* DESCRIPTION:                                                                         */
/*  Sets or changes the LifeVibes module parameters.                                    */
/*                                                                                      */
/* PARAMETERS:                                                                          */
/*  hInstance               Instance handle                                             */
/*  pParams                 Pointer to a parameter structure                            */
/*                                                                                      */
/* RETURNS:                                                                             */
/*  LVM_SUCCESS             Succeeded                                                   */
/*  LVM_NULLADDRESS         When hInstance, pParams or any control pointers are NULL    */
/*  LVM_OUTOFRANGE          When any of the control parameters are out of range         */
/*                                                                                      */
/* NOTES:                                                                               */
/*  1.  This function may be interrupted by the LVM_Process function                    */
/*                                                                                      */
/****************************************************************************************/
#ifdef __DLL_EXPORT
__declspec(dllexport)
#endif /* __DLL_EXPORT */
LVM_ReturnStatus_en LVM_SetControlParameters(LVM_Handle_t           hInstance,
                                             LVM_ControlParams_t    *pParams);


/****************************************************************************************/
/*                                                                                      */
/* FUNCTION:                LVM_Process                                                 */
/*                                                                                      */
/* DESCRIPTION:                                                                         */
/*  Process function for the LifeVibes module.                                          */
/*                                                                                      */
/* PARAMETERS:                                                                          */
/*  hInstance               Instance handle                                             */
/*  pInData                 Pointer to the input data                                   */
/*  pOutData                Pointer to the output data                                  */
/*  NumSamples              Number of samples in the input buffer                       */
/*  AudioTime               Audio Time of the current input data in milli-seconds       */
/*                                                                                      */
/* RETURNS:                                                                             */
/*  LVM_SUCCESS            Succeeded                                                    */
/*  LVM_INVALIDNUMSAMPLES  When the NumSamples is not a valied multiple in unmanaged    */
/*                         buffer mode                                                  */
/*  LVM_ALIGNMENTERROR     When either the input our output buffers are not 32-bit      */
/*                         aligned in unmanaged mode                                    */
/*  LVM_NULLADDRESS        When one of hInstance, pInData or pOutData is NULL           */
/*                                                                                      */
/* NOTES:                                                                               */
/*  1. The input and output buffers must be 32-bit aligned                              */
/*  2. Number of samples is defined as follows:                                         */
/*      MONO                the number of samples in the block                          */
/*      MONOINSTEREO        the number of sample pairs in the block                     */
/*      STEREO              the number of sample pairs in the block                     */
/*                                                                                      */
/*   3. If Crossover Disable, pOutData[0] MUST be initialize as a non-null pointer      */
/*   4. If Crossover Enable, pOutData[0] & pOutData[1] MUST be initialize as            */
/*      a non-null pointer                                                              */
/*                                                                                      */
/*                                                                                      */
/*                                                                                      */
/*                                                                                      */
/****************************************************************************************/
#ifdef __DLL_EXPORT
__declspec(dllexport)
#endif /* __DLL_EXPORT */
LVM_ReturnStatus_en LVM_Process(LVM_Handle_t                hInstance,
                                const LVM_INT16             *pInData,
                                LVM_INT16                   **pOutData,
                                LVM_UINT16                  NumSamples,
                                LVM_UINT32                  AudioTime);


#ifdef ALGORITHM_AVL
/****************************************************************************************/
/*                                                                                      */
/* FUNCTION:                LVM_GetAVLGain                                              */
/*                                                                                      */
/* DESCRIPTION:                                                                         */
/*  This function is used to retrieve the AVL last generated gain in Q16.15             */
/*  linear values.                                                                      */
/*                                                                                      */
/* PARAMETERS:                                                                          */
/*  hInstance               Instance Handle                                             */
/*  pAVL_Gain               Pointer to the gain                                         */
/*                                                                                      */
/* RETURNS:                                                                             */
/*  LVM_SUCCESS             Succeeded                                                   */
/*  LVM_NULLADDRESS         When hInstance or pAVL_Gain are null addresses              */
/*                                                                                      */
/* NOTES:                                                                               */
/*  1.  This function may be interrupted by the LVM_Process function                    */
/*                                                                                      */
/****************************************************************************************/
#ifdef __DLL_EXPORT
__declspec(dllexport)
#endif /* __DLL_EXPORT */
LVM_ReturnStatus_en LVM_GetAVLGain( LVM_Handle_t    hInstance,
                                    LVM_INT32       *pAVL_Gain);


#endif /* ALGORITHM_AVL */

#ifdef ALGORITHM_EQNB
/****************************************************************************************/
/*                                                                                      */
/* FUNCTION:                LVM_SetHeadroomParams                                       */
/*                                                                                      */
/* DESCRIPTION:                                                                         */
/*  This function is used to set the automatic headroom management parameters.         */
/*                                                                                      */
/* PARAMETERS:                                                                          */
/*  hInstance               Instance Handle                                             */
/*  pHeadroomParams         Pointer to headroom parameter structure                     */
/*                                                                                      */
/* RETURNS:                                                                             */
/*  LVM_SUCCESS             Succeeded                                                   */
/*                                                                                      */
/* NOTES:                                                                               */
/*  1.  This function may be interrupted by the LVM_Process function                    */
/*                                                                                      */
/****************************************************************************************/
#ifdef __DLL_EXPORT
__declspec(dllexport)
#endif /* __DLL_EXPORT */
LVM_ReturnStatus_en LVM_SetHeadroomParams(  LVM_Handle_t            hInstance,
                                            LVM_HeadroomParams_t    *pHeadroomParams);

/****************************************************************************************/
/*                                                                                      */
/* FUNCTION:                LVM_GetHeadroomParams                                       */
/*                                                                                      */
/* DESCRIPTION:                                                                         */
/*  This function is used to get the automatic headroom management parameters.          */
/*                                                                                      */
/* PARAMETERS:                                                                          */
/*  hInstance               Instance Handle                                             */
/*  pHeadroomParams         Pointer to headroom parameter structure (output)            */
/*                                                                                      */
/* RETURNS:                                                                             */
/*  LVM_SUCCESS             Succeeded                                                   */
/*  LVM_NULLADDRESS         When hInstance or pHeadroomParams are NULL                  */
/*                                                                                      */
/* NOTES:                                                                               */
/*  1.  This function may be interrupted by the LVM_Process function                    */
/*                                                                                      */
/****************************************************************************************/
#ifdef __DLL_EXPORT
__declspec(dllexport)
#endif /* __DLL_EXPORT */
LVM_ReturnStatus_en LVM_GetHeadroomParams(  LVM_Handle_t            hInstance,
                                            LVM_HeadroomParams_t    *pHeadroomParams);
#endif /* ALGORITHM_EQNB */

#ifdef ALGORITHM_PSA
/****************************************************************************************/
/*                                                                                      */
/* FUNCTION:                LVM_GetSpectrum                                             */
/*                                                                                      */
/* DESCRIPTION:                                                                         */
/* This function is used to retrieve Spectral information at a given Audio time         */
/* for display usage                                                                    */
/*                                                                                      */
/* PARAMETERS:                                                                          */
/*  hInstance               Instance Handle                                             */
/*  pCurrentPeaks           Pointer to location where currents peaks are to be saved    */
/*  pPastPeaks              Pointer to location where past peaks are to be saved        */
/*  pCentreFreqs            Pointer to location where centre frequency of each band is  */
/*                          to be saved                                                 */
/*  AudioTime               Audio time at which the spectral information is needed      */
/*                                                                                      */
/* RETURNS:                                                                             */
/*  LVM_SUCCESS             Succeeded                                                   */
/*  LVM_NULLADDRESS         If any of input addresses are NULL                          */
/*  LVM_WRONGAUDIOTIME      Failure due to audio time error                             */
/*                                                                                      */
/* NOTES:                                                                               */
/*  1. This function may be interrupted by the LVM_Process function                     */
/*                                                                                      */
/****************************************************************************************/
#ifdef __DLL_EXPORT
__declspec(dllexport)
#endif /* __DLL_EXPORT */
LVM_ReturnStatus_en LVM_GetSpectrum( LVM_Handle_t            hInstance,
                                     LVM_INT8                *pCurrentPeaks,
                                     LVM_INT8                *pPastPeaks,
                                     LVM_UINT16              *pCentreFreqs,
                                     LVM_UINT32              AudioTime);


#endif /* ALGORITHM_PSA */


/****************************************************************************************/
/*                                                                                      */
/* FUNCTION:                LVM_SetVolumeNoSmoothing                                    */
/*                                                                                      */
/* DESCRIPTION:                                                                         */
/* This function is used to set output volume without any smoothing                     */
/*                                                                                      */
/* PARAMETERS:                                                                          */
/*  hInstance               Instance Handle                                             */
/*  pParams                 Control Parameters, only volume value is used here          */
/*                                                                                      */
/* RETURNS:                                                                             */
/*  LVM_SUCCESS             Succeeded                                                   */
/*  LVM_NULLADDRESS         If any of input addresses are NULL                          */
/*  LVM_OUTOFRANGE          When any of the control parameters are out of range         */
/*                                                                                      */
/* NOTES:                                                                               */
/*  1. This function may be interrupted by the LVM_Process function                     */
/*                                                                                      */
/****************************************************************************************/
#ifdef __DLL_EXPORT
__declspec(dllexport)
#endif /* __DLL_EXPORT */
LVM_ReturnStatus_en LVM_SetVolumeNoSmoothing( LVM_Handle_t           hInstance,
                                              LVM_ControlParams_t    *pParams);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif      /* __LVM_H__ */

