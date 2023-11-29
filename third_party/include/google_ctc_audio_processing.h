#ifndef GOOGLE_RTC_AUDIO_PROCESSING_H
#define GOOGLE_RTC_AUDIO_PROCESSING_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GoogleCtcAudioProcessingState GoogleCtcAudioProcessingState;

// Creates an instance of GoogleCtcAudioProcessing with the tuning embedded in
// the library. If creation fails, NULL is returned.
GoogleCtcAudioProcessingState *GoogleCtcAudioProcessingCreate(void);

// Frees all allocated resources in `state`.
void GoogleCtcAudioProcessingFree(GoogleCtcAudioProcessingState *state);

// Apply CTC on `src` and produce result in `dest`.
int GoogleCtcAudioProcessingProcess(GoogleCtcAudioProcessingState *const state,
                                    const int16_t *const src,
                                    int16_t *const dest);

#ifdef __cplusplus
}
#endif

#endif  // GOOGLE_RTC_AUDIO_PROCESSING_H
