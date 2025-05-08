/*  Copyright 2004-2025 NXP
 *
 * SPDX-License-Identifier: MIT
 * This license applies ONLY to this header file LVC_Types.h
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

/****************************************************************************************

     $Author: beq07720 $
     $Revision: 130089 $
     $Date: 2019-07-24 07:46:43 +0200 (Wed, 24 Jul 2019) $

*****************************************************************************************/

/** @file
 *  Header file defining the standard LifeVibes types for use in the application layer
 *  interface of all LifeVibes modules
 */

#ifndef LVC_TYPES_H
#define LVC_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/****************************************************************************************/
/*                                                                                      */
/*  definitions                                                                         */
/*                                                                                      */
/****************************************************************************************/

#define LVM_NULL                (void *)0           ///< NULL pointer

#define LVM_TRUE                1                   ///< Boolean True
#define LVM_FALSE               0                   ///< Boolean False

#define LVM_MAXINT_8            127                 ///< Maximum positive integer size
#define LVM_MAXINT_16           32767               ///< Maximum signed int 16 bits number
#define LVM_MAXUINT_16          65535U              ///< Maximum un-signed int 16 bits number
#define LVM_MAXINT_32           2147483647          ///< Maximum signed int 32 bits number
#define LVM_MAXUINT_32          4294967295U         ///< Maximum un-signed int 32 bits number
#define LVM_MININT_32           0x80000000U         ///< Minimum signed int 32 bit number in 2's complement form

#define LVM_MAXENUM             2147483647          ///< Maximum value for enumerator
#define LVM_MODULEID_MASK       0xFF00              ///< Mask to extract the calling module ID from callbackId
#define LVM_EVENTID_MASK        0x00FF              ///< Mask to extract the callback event from callbackId

/* Memory table*/
#define LVM_MEMREGION_PERSISTENT_SLOW_DATA      0   ///< Offset to the instance memory region
#define LVM_MEMREGION_PERSISTENT_FAST_DATA      1   ///< Offset to the persistent data memory region
#define LVM_MEMREGION_PERSISTENT_FAST_COEF      2   ///< Offset to the persistent coefficient memory region
#define LVM_MEMREGION_TEMPORARY_FAST            3   ///< Offset to temporary memory region

#define LVM_NR_MEMORY_REGIONS                   4   ///< Number of memory regions

#define LVM_MODE_LVWIREFORMAT_LENGTH (4) ///< Number of bytes to encode @ref LVM_Mode_en in LVWireFormat
#define LVM_CONFIG_LVWIREFORMAT_LENGTH (4) ///< Number of bytes to encode @ref LVM_Config_en in LVWireFormat
#define LVM_FS_LVWIREFORMAT_LENGTH (4) ///< Number of bytes to encode @ref LVM_Fs_en sample Rate in LVWireFormat
#define LVM_CHANNELTYPE_LVWIREFORMAT_LENGTH (4) ///< Number of bytes to encode @ref LVM_ChannelType_en in LVWireFormat

#define LVM_CHAR_LVWIREFORMAT_LENGTH   (1) ///< Number of bytes to encode ASCII character in LVWireFormat
#define LVM_INT8_LVWIREFORMAT_LENGTH   (1) ///< Number of bytes to encode Signed 8-bit word in LVWireFormat
#define LVM_UINT8_LVWIREFORMAT_LENGTH   (1) ///< Number of bytes to encode Unsigned 8-bit word in LVWireFormat

#define LVM_INT16_LVWIREFORMAT_LENGTH   (2) ///< Number of bytes to encode Signed 16-bit word in LVWireFormat
#define LVM_UINT16_LVWIREFORMAT_LENGTH   (2) ///< Number of bytes to encode Unsigned 16-bit word in LVWireFormat

#define LVM_INT32_LVWIREFORMAT_LENGTH   (4) ///< Number of bytes to encode Signed 32-bit word in LVWireFormat
#define LVM_UINT32_LVWIREFORMAT_LENGTH   (4) ///< Number of bytes to encode Unsigned 32-bit word in LVWireFormat


/****************************************************************************************/
/*                                                                                      */
/*  Basic types                                                                         */
/*                                                                                      */
/****************************************************************************************/

typedef     char                LVM_CHAR;           ///< ASCII character

typedef     char                LVM_INT8;           ///< Signed 8-bit word
typedef     unsigned char       LVM_UINT8;          ///< Unsigned 8-bit word

typedef     short               LVM_INT16;          ///< Signed 16-bit word
typedef     unsigned short      LVM_UINT16;         ///< Unsigned 16-bit word

#if (defined __LP64__) && __LP64__
typedef     int                 LVM_INT32;          ///< Signed 32-bit word
typedef     unsigned int        LVM_UINT32;         ///< Unsigned 32-bit word
// Type macros
#define LVM_PRINTF_FORMAT_SPECIFIER_INT32 "d"
#define LVM_PRINTF_FORMAT_SPECIFIER_UINT32 "u"
#elif defined(TOOLCHAIN_ADK)                        // CSR ADK
typedef     int                 LVM_INT32;          ///< Signed 32-bit word
typedef     unsigned int        LVM_UINT32;         ///< Unsigned 32-bit word
// Type macros
#define LVM_PRINTF_FORMAT_SPECIFIER_INT32 "d"
#define LVM_PRINTF_FORMAT_SPECIFIER_UINT32 "u"
#else // (defined __LP64__) && __LP64__
typedef     long                 LVM_INT32;         ///< Signed 32-bit word
typedef     unsigned long        LVM_UINT32;        ///< Unsigned 32-bit word
// Type macros
#define LVM_PRINTF_FORMAT_SPECIFIER_INT32 "li"
#define LVM_PRINTF_FORMAT_SPECIFIER_UINT32 "lu"
#endif // (defined __LP64__) && __LP64__

typedef     float                 LVM_FLOAT;          ///< Single Precision floating point type
typedef     double                LVM_DOUBLE;         ///< Double Precision floating point type

/****************************************************************************************/
/*                                                                                      */
/*  Standard Enumerated types                                                           */
/*                                                                                      */
/****************************************************************************************/

/**
The @ref LVM_Mode_en enumerated type is used to set the operating mode of a particular feature inside the LifeVibes modules.
The feature can be separately set to enable the feature processing (i.e., ON) or to disable all feature processing
modules (i.e., OFF).
*/
typedef enum
{
    LVM_MODE_OFF    = 0,     ///< LVM module disabled
    LVM_MODE_ON     = 1,     ///< LVM module enabled
	LVM_MODE_MUTE   = 2,     ///< LVM module muted
    LVM_MODE_DUMMY  = LVM_MAXENUM
} LVM_Mode_en;

/**
Sets stream Format
*/
typedef enum
{
    LVM_STEREO          = 0,           ///<Stereo stream
    LVM_MONOINSTEREO    = 1,           ///<Mono in stereo stream
    LVM_MONO            = 2,           ///<Mono stream
    LVM_5DOT1           = 3,           ///<stream 5.1 formatted
    LVM_7DOT1           = 4,           ///<stream 7.1 formatted
    LVM_SOURCE_DUMMY    = LVM_MAXENUM
} LVM_Format_en;

/**
Sets Speaker type
*/
typedef enum
{
    LVM_SPEAKER_MONO    = 0,        ///< Mono type speaker
    LVM_SPEAKER_STEREO  = 1,        ///< Stereo type speaker
    LVM_SPEAKER_DUMMY   = LVM_MAXENUM
} LVM_SpeakerType_en;

/**
Sets Word length
*/
typedef enum
{
    LVM_16_BIT      = 0,           ///< 16 bit word length
    LVM_32_BIT      = 1,           ///< 32 bit word length
    LVM_WORDLENGTH_DUMMY = LVM_MAXENUM
} LVM_WordLength_en;

/**
The LVM product supports the sample rates specified in @ref LVM_Fs_en. The input and output sample rates are always the same.
*/
typedef enum
{
    LVM_FS_8000  = 0,              ///< 8k sampling rate
    LVM_FS_11025 = 1,              ///< 11.025k sampling rate
    LVM_FS_12000 = 2,              ///< 12k sampling rate
    LVM_FS_16000 = 3,              ///< 16k sampling rate
    LVM_FS_22050 = 4,              ///< 22.050k sampling rate
    LVM_FS_24000 = 5,              ///< 24k sampling rate
    LVM_FS_32000 = 6,              ///< 32k sampling rate
    LVM_FS_44100 = 7,              ///< 44.1k sampling rate
    LVM_FS_48000 = 8,              ///< 48k sampling rate
	LVM_FS_96000 = 9,
    LVM_FS_COUNT = 10,             ///< Max sampling rate count
    LVM_FS_INVALID = LVM_MAXENUM-1,
    LVM_FS_DUMMY = LVM_MAXENUM
} LVM_Fs_en;

/**
The LVM product supports the sample rates specified in @ref LVM_Fs_HDAudio_en. The input and output sample rates are always the same.
*/
typedef enum
{
    LVM_FS_HDAUDIO_8000  = 0,              ///< 8k sampling rate
    LVM_FS_HDAUDIO_11025 = 1,              ///< 11.025k sampling rate
    LVM_FS_HDAUDIO_16000 = 2,              ///< 16k sampling rate
    LVM_FS_HDAUDIO_22050 = 3,              ///< 22.050k sampling rate
    LVM_FS_HDAUDIO_32000 = 4,              ///< 32k sampling rate
    LVM_FS_HDAUDIO_44100 = 5,              ///< 44.1k sampling rate
    LVM_FS_HDAUDIO_48000 = 6,              ///< 48k sampling rate
    LVM_FS_HDAUDIO_88200 = 7,              ///< 88.2k sampling rate
    LVM_FS_HDAUDIO_96000 = 8,              ///< 96k sampling rate
    LVM_FS_HDAUDIO_176400 = 9,             ///< 176.4k sampling rate
    LVM_FS_HDAUDIO_192000 = 10,            ///< 192k sampling rate
    LVM_FS_HDAUDIO_COUNT = 11,             ///< Max sampling rate count
    LVM_FS_HDAUDIO_INVALID = LVM_MAXENUM-1,
    LVM_FS_HDAUDIO_DUMMY = LVM_MAXENUM
} LVM_Fs_HDAudio_en;

/**
The @ref LVM_PcmFormat_en enumerated type identifies the different PCM formats that can be configured
*/
typedef enum {
    LVM_PCMFORMAT_16_BIT             = 0, ///< PCM signed Q0.15
    LVM_PCMFORMAT_32_BIT             = 1, ///< PCM signed Q0.31
    LVM_PCMFORMAT_8_24_BIT           = 2, ///< PCM signed Q7.24
    LVM_PCMFORMAT_COUNT              = 3, ///< Max PCM Format count
    LVM_PCMFORMAT_DUMMY              = LVM_MAXENUM
} LVM_PcmFormat_en;

/**
The enumerated type is used to select the reset mode for the module.
@ref LVM_RESET_SOFT is used to select a soft reset (or partial reset) and @ref LVM_RESET_HARD is
used to select a hard reset (full re-initialization).
*/
typedef enum
{
/**
<table border>
   <tr>
       <td><b>Name</b></td>
       <td><b>MODE</b></td>
   </tr>
   <tr>
       <td>ResetType</td>
       <td>@ref LVM_RESET_SOFT</td>
   </tr>
   <tr>
       <td></td>
       <td>@ref LVM_RESET_HARD</td>
   </tr>
</table>
*/
    LVM_RESET_SOFT    = 0,         ///< Reset type for LVM where a partial reset of the module should be performed
    LVM_RESET_HARD    = 1,         ///< Reset type for LVM where a full reset of the module should be performed
    LVM_RESET_DUMMY   = LVM_MAXENUM
} LVM_ResetType_en;

/**
The @ref LVM_MemoryTypes_en enumerated type identifies the memory region types so that they can be correctly placed in memory
by the calling application.
The module initially has no permanent memory storage and makes no use of persistent memory allocation internally.
The calling application must allocate memory for the module to use.

Four memory regions are required:
@li @ref LVM_MEMREGION_PERSISTENT_SLOW_DATA : this type of memory is used to store all the control data that needs to be saved between two consecutive calls to the process function.
@li @ref LVM_MEMREGION_PERSISTENT_FAST_DATA : this type of memory is used to store data such as filter history
@li @ref LVM_MEMREGION_PERSISTENT_FAST_COEF : this type of memory is used to store filter coefficients.
@li @ref LVM_MEMREGION_TEMPORARY_FAST (scratch): this type of memory is used to store temporary data. This memory can be reused by the application in between calls to the process function.

This collection of memory regions forms the module instance.

Typically the memory is allocated by the application dynamically; however, it can be allocated statically if required.
The sizes of the memory regions can be found by running the GetMemoryTable functions on a simulator and noting
the returned values. Alternatively contact NXP who can provide the figures.
It is possible that these memory sizes will change between release versions of the library and hence the dynamic memory allocation method is preferred where possible.
On some target platforms the placement of memory regions is critical for achieving optimal performance of the module.
*/
typedef enum
{
    LVM_PERSISTENT_SLOW_DATA    = LVM_MEMREGION_PERSISTENT_SLOW_DATA,       ///< Persistent slow memory region
    LVM_PERSISTENT_FAST_DATA    = LVM_MEMREGION_PERSISTENT_FAST_DATA,       ///< Persistent fast memory region
    LVM_PERSISTENT_FAST_COEF    = LVM_MEMREGION_PERSISTENT_FAST_COEF,       ///< Persisten fast memory for coefficient storage
    LVM_TEMPORARY_FAST          = LVM_MEMREGION_TEMPORARY_FAST,             ///< Temporary fast memory region
    LVM_MEMORYTYPE_DUMMY        = LVM_MAXENUM
} LVM_MemoryTypes_en;

/**
Sets mod of Configuration
*/
typedef enum
{
    LVM_CONFIG_HANDSET                       = 0,                  ///< Handset configuration
    LVM_CONFIG_SPEAKERPHONE                  = 1,                  ///< Speaker mod configuration
    LVM_CONFIG_STEREOHEADSET                 = 2,                  ///< Stereo Headset configuration
    LVM_CONFIG_HEADSET_ENDFIRE               = 3,                  ///< Close-microphone endfire headset configuration
    LVM_CONFIG_HEADSET_CALLING               = 4,                  ///< Ear cup mics for calling
    LVM_CONFIG_HEADSET_CALLING_BOOM          = 5,                  ///< Ear cup and boom mics for calling
    LVM_CONFIG_HEADSET_FRONTEND              = 6,                  ///< Ear cup mics for frontend pre enhancement
    LVM_CONFIG_HEADSET_FRONTEND_BOOM         = 7,                  ///< Ear cup and boom mics for frontend pre enhancement
    LVM_CONFIG_DUMMY                         = LVM_MAXENUM
} LVM_Config_en;

/**
Sets Channel Type
*/
typedef enum
{
    LVM_MICROPHONE_CHANNEL        = 0,                  ///< Microphone Channel
    LVM_SPEAKER_CHANNEL           = 1,                  ///< Speaker Channel
    LVM_SPEAKER_AS_MIC_CHANNEL    = 2,                  ///< Speaker or Receiver behaving as a Microphone Channel
    LVM_MICROPHONE_AS_FAREND_REFERENCE_CHANNEL = 3,     ///< Microphone as Farend Reference Channel
    LVM_CHANNEL_DUMMY             = LVM_MAXENUM
} LVM_ChannelType_en;

/**
Sets Tunability of tuning parameters
*/
typedef enum
{
    LVM_TUNABILITY_BASIC     = 0,                  ///< Basic tuning parameter, always visible
    LVM_TUNABILITY_ADVANCED  = 1,                  ///< Expert tuning parameter, only visible in advanced tuning mode
    LVM_TUNABILITY_RESERVED  = 2,                  ///< Reserved tuning parameter, not visible
    LVM_TUNABILITY_DUMMY     = LVM_MAXENUM
} LVM_Tunability_en;

/**
Sets Volume Dependence
*/
typedef enum
{
    LVM_VOLUME_DEPENDENT          = 0,                  ///< Volume dependent
    LVM_VOLUME_INDEPENDENT        = 1,                  ///< Volume independent
    LVM_VOLUMEDEPENDENCE_DUMMY    = LVM_MAXENUM
} LVM_VolumeDependence_en;

/**
The @ref LVM_MemoryRegion_st type defines a memory region by specifying its size in bytes, its region type and its base pointer.
@see LVM_MemoryTypes_en
*/
typedef struct
{
    LVM_UINT32                  Size;                   ///< The size of the memory region in bytes
    LVM_MemoryTypes_en          Type;                   ///< Type of memory region
    void                        *pBaseAddress;          ///< Pointer to the memory region base address
} LVM_MemoryRegion_st;

/**
The LVM_MemoryTable_st type defines the memory requirements of the module as an array of region definitions.
The number of required memory regions is given by the constant @ref LVM_NR_MEMORY_REGIONS
@see LVM_MemoryRegion_st
*/
typedef struct
{
    LVM_MemoryRegion_st         Region[LVM_NR_MEMORY_REGIONS];  ///< One definition of all memory regions
} LVM_MemoryTable_st;

/**
The LVM_ContextTable_st type defines a memory region by specifying its size in bytes and its base pointer.
@see LVM_ContextTable_st
*/
typedef struct
{
    LVM_UINT32 ContextTableLength;  ///< its size in bytes
    LVM_CHAR   *pContext;           ///< its base pointer
} LVM_ContextTable_st;

/**
Beats Per Minute Structure
*/
typedef struct
{
    LVM_INT16                   ShortTermMinimum;       ///< Beats per minute in Q9.6 format
    LVM_INT16                   ShortTermAverage;       ///< Beats per minute in Q9.6 format
    LVM_INT16                   ShortTermMaximum;       ///< Beats per minute in Q9.6 format

    LVM_INT16                   Confidence;             ///< Beat confidence level: 0 = no confidence, 32767 = maximum confidence
    LVM_INT16                   Strength;               ///< Beat strength level:   0 = no beat, 32767 = maximum strength beat
    LVM_INT16                   LongTermMinimum;        ///< Beats per minute in Q9.6 format
    LVM_INT16                   LongTermAverage;        ///< Beats per minute in Q9.6 format
    LVM_INT16                   LongTermMaximum;        ///< Beats per minute in Q9.6 format

} LVM_BPMModuleStats_st;


/****************************************************************************************/
/*                                                                                      */
/*  Standard Function Prototypes                                                        */
/*                                                                                      */
/****************************************************************************************/
/**
@brief General purpose callback function

@param pCallbackData           Pointer to the callback data structure
@param pGeneralPurpose         General purpose pointer (e.g. to a data structure needed in the callback)
@param PresetLength            General purpose variable (e.g. to be used as callback ID)
@return \ref LVM_INT32
*/
typedef LVM_INT32 (*LVM_Callback)(void          *pCallbackData,
                                  void          *pGeneralPurpose,
                                  LVM_INT16     GeneralPurpose );

/****************************************************************************************/
/*                                                                                      */
/*  End of file                                                                         */
/*                                                                                      */
/****************************************************************************************/


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* LVC_TYPES_H */
