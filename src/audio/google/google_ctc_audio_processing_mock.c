#include <rtos/alloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "google_ctc_audio_processing.h"
#include "ipc/topology.h"

struct GoogleCtcAudioProcessingState {
  int num_frames;
  int partition_size;
  int impulse_size;
  int chunk_frames;
  int sample_rate;
  int is_symmetric;
};

GoogleCtcAudioProcessingState *GoogleCtcAudioProcessingCreate(void) {
  struct GoogleCtcAudioProcessingState *s =
      rballoc(0, SOF_MEM_CAPS_RAM, sizeof(GoogleCtcAudioProcessingState));
  if (!s) return NULL;

  s->num_frames = 64;
  s->partition_size = 64;
  s->impulse_size = 256;
  s->chunk_frames = 480;
  s->sample_rate = 48000;
  s->is_symmetric = 0;
  return s;
}

void GoogleCtcAudioProcessingFree(GoogleCtcAudioProcessingState *state) {
  if (state != NULL) {
    rfree(state);
  }
}

int GoogleCtcAudioProcessingProcess(GoogleCtcAudioProcessingState *const state,
                                    const int16_t *const src,
                                    int16_t *const dest) {
  memcpy(dest, src, sizeof(int16_t) * state->num_frames);
  return 0;
}
