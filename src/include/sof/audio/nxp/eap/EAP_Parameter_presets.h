#ifndef EAP_PARAMETER_PRESETS_H_
#define EAP_PARAMETER_PRESETS_H_

#include <nxp/eap/EAP_Includes/EAP16.h>

#ifndef CONFIG_COMP_NXP_EAP_STUB
#include <Example_Application/CONFIG_HEADER/EAP_Parameter_AllEffectOff.h>
#include <Example_Application/CONFIG_HEADER/EAP_Parameter_AutoVolumeLeveler.h>
#include <Example_Application/CONFIG_HEADER/EAP_Parameter_ConcertSound.h>
#include <Example_Application/CONFIG_HEADER/EAP_Parameter_LoudnessMaximiser.h>
#include <Example_Application/CONFIG_HEADER/EAP_Parameter_MusicEnhancerRMSLimiter.h>
#include <Example_Application/CONFIG_HEADER/EAP_Parameter_VoiceEnhancer.h>

#else

uint8_t InstParams_allEffectOff[28] = {};
uint8_t ControlParamSet_allEffectOff[216] = {};
uint8_t ControlParamSet_autoVolumeLeveler[216] = {};
uint8_t ControlParamSet_concertSound[216] = {};
uint8_t ControlParamSet_loudnessMaximiser[216] = {};
uint8_t ControlParamSet_musicEnhancerRmsLimiter[216] = {};
uint8_t ControlParamSet_voiceEnhancer[216] = {};
#endif

#endif /* EAP_PARAMETER_PRESETS_H_ */
