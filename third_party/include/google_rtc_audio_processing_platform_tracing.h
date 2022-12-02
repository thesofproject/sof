// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Google LLC.
//
// Author: Per Ahgren <peah@google.com>
#ifndef GOOGLE_RTC_AUDIO_PROCESSING_PLATFORM_TRACING_H_
#define GOOGLE_RTC_AUDIO_PROCESSING_PLATFORM_TRACING_H_

#include <stdint.h>

// Provides functions faciliting limited basic tracing capabilities for
// debugging. The main targets for this are embedded platforms, where the
// platform only provides basic tracing capabilities.
// This header only provides the definition of the methods, and it is up to the
// platform to provide the implementations.

#ifdef __cplusplus
extern "C" {
#endif

// Provides basic tracing output for debugging, producing a trace line
// containing the content in `value`. The format of the trace line is
// platform-dependent.
void GoogleRtcTraceInt(int value);

// Provides basic tracing output for debugging, producing a trace line
// containing the content in `value` and then triggering an abort, or exit. The
// format of the trace line is platform-dependent.
void GoogleRtcTraceIntAndAbort(int value);

// Provides basic tracing output for debugging, producing a trace line
// containing the  address in `address`. The format of the trace line is
// platform-dependent.
void GoogleRtcTraceAddress(uintptr_t memory_address);

#ifdef __cplusplus
}
#endif

#endif  // GOOGLE_RTC_AUDIO_PROCESSING_PLATFORM_TRACING_H_
