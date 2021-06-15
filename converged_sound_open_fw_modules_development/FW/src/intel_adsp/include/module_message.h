// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.


#ifndef ADSP_FW_COMMON_MODULE_GENERIC_EFFECTS_H
#define ADSP_FW_COMMON_MODULE_GENERIC_EFFECTS_H

#include <syntax_sugar.h>
#include <stdint.h>

//! Reset all effects (set by MM_TYPE_EFFECT_*
static const int32_t MM_TYPE_RESET_ALL_EFFECTS  = 0;
//! Amplification effect.
/*!
  For C++: Payload should be casted to effects::MMAmplify;
  For C: Payload should be casted to effects_MMAmplify;
 */
static const int32_t MM_TYPE_EFFECT_AMPLIFY     = 1;

static const int32_t MM_TARGET_TYPE_ALL             = 0xff;
static const int32_t MM_TARGET_TYPE_BASE_FW         = 0;
static const int32_t MM_TARGET_TYPE_MIX_IN          = 1;
static const int32_t MM_TARGET_TYPE_MIX_OUT         = 2;
static const int32_t MM_TARGET_TYPE_COPIER          = 3;
static const int32_t MM_TARGET_TYPE_PEAK_VOL        = 4;
static const int32_t MM_TARGET_TYPE_UP_DOWN_MIXER   = 5;
static const int32_t MM_TARGET_TYPE_MUX             = 6;
static const int32_t MM_TARGET_TYPE_SRC             = 7;
static const int32_t MM_TARGET_TYPE_WOV             = 8;
static const int32_t MM_TARGET_TYPE_FX              = 9;
static const int32_t MM_TARGET_TYPE_AEC             = 10;
static const int32_t MM_TARGET_TYPE_KPB             = 11;
static const int32_t MM_TARGET_TYPE_MICSEL          = 12;
static const int32_t MM_TARGET_TYPE_FXF             = 13; //i.e.SmartAmp
static const int32_t MM_TARGET_TYPE_AUDCLASS        = 14;
//static const int32_t MM_TARGET_TYPE_eFakeCopierModule= 15;
//static const int32_t MM_TARGET_TYPE_eIoDriver =16;
static const int32_t MM_TARGET_TYPE_WHM             = 17;
//static const int32_t MM_TARGET_TYPE_eGdbStubModule= 18;
static const int32_t MM_TARGET_TYPE_SENSING         = 19;

//! Left to right, stop on first success
static const int32_t MM_PA_L2R_F = 0;
//! Right to left, stop on first success
static const int32_t MM_PA_R2L_F = 1;
//! Left to right, continue
static const int32_t MM_PA_L2R_C = 2;
//! Right to left, continue
static const int32_t MM_PA_R2L_C = 3;

typedef struct
{
    //! Message ID, one of MM_TYPE_*
    int32_t type        : 8;
    //! Depth of propagation (in number of pipelines priorities).
    /*!
      0 means to propagate message ONLY for module(s) on parent pipeline of
      message sender,
      X means to propagate message for module(s) on parent pipeline of message
      sender AND X levels up (forward),
      -X means to propagate message for module(s) on parent pipeline of message
      sender AND X levels down (backward),
    */
    int32_t prop_depth  : 4;
    //! Order of propagation, one of MM_PA_*
    /*!
      Determines order of propagation message to modules within pipeline.
      Does not determine order of operation of pipelines (assumend BFS with
      direction set by prop_depth).
    */
    int32_t prop_order  : 4;
    int32_t rsvd        : 16;
} ModuleMessageType, mod_msg_type_s;
C_ASSERT(sizeof(ModuleMessageType) == 4);

typedef struct
{
    //! Message Target Type, one of MM_TARGET_TYPE_*
    int32_t target_type;
    //! Module message type
    ModuleMessageType message_type;
    //! Size of payload (in bytes)
    uint32_t payload_size;
    //! Beginng of payload. Size of pyaload_data depends on payload_size.
    /*!
      Payload structure depends on message_type.
    */
    uint8_t payload_data[0];
} ModuleMessage, mod_message_s;
C_ASSERT(sizeof(ModuleMessage) == 12);

#ifdef __cplusplus

template<typename T> struct TModuleMessage: public ModuleMessage { T payload; };

#endif // #ifdef __cplusplus

#ifdef __cplusplus
namespace effects
{
#endif // __cplusplus

typedef struct
{
    //! Amplify value in exp scale
    /*!
      new_value = current_value * power(.5, attenuation_value);
    */
    int32_t attenuation_value;
} MMAmplify, effects_MMAmplify;

#ifdef __cplusplus
} // namespace effects
#endif // __cplusplus

#endif // ADSP_FW_COMMON_MODULE_GENERIC_EFFECTS_H
