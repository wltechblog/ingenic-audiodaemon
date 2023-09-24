#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <imp/imp_audio.h>
#include <imp/imp_log.h>

#define AO_SAMPLE_RATE AUDIO_SAMPLE_RATE_48000  // Default sample rate for audio output; can be changed
#define AO_NUM_PER_FRM compute_numPerFrm(AO_SAMPLE_RATE)
#define AO_MAX_FRAME_SIZE 1280
#define CHN_VOL 100
#define AO_GAIN 24

// Functions
void reinitialize_audio_device(int devID);
void *ao_test_play_thread(void *arg);
void pause_audio_output(int devID, int chnID);
void clear_audio_output_buffer(int devID, int chnID);
void resume_audio_output(int devID, int chnID);
void flush_audio_output_buffer(int devID, int chnID);

#endif // AUDIO_H
