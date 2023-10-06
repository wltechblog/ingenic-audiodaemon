#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "imp/imp_audio.h"
#include "imp/imp_log.h"
#include "audio_common.h"
#include "config.h"
#include "cJSON.h"
#include "output.h"
#include "logging.h"
#include "utils.h"

#define TRUE 1
#define TAG "AO"

// Global variable to hold the maximum frame size for audio output.
int g_ao_max_frame_size = DEFAULT_AO_MAX_FRAME_SIZE;

/**
 * Set the global maximum frame size for audio output.
 * @param frame_size The desired frame size.
 */
void set_ao_max_frame_size(int frame_size) {
    g_ao_max_frame_size = frame_size;
}

/**
 * Handles errors and reinitializes the audio device.
 * @param aoDevID Device ID.
 * @param aoChnID Channel ID.
 * @param errorMsg Error message to be handled.
 */
void handle_and_reinitialize_output(int aoDevID, int aoChnID, const char *errorMsg) {
    handle_audio_error(errorMsg);
    reinitialize_audio_output_device(aoDevID, aoChnID);
}

/**
 * Initializes the audio device using the attributes from the configuration.
 * @param aoDevID Device ID.
 * @param aoChnID Channel ID.
 */
void initialize_audio_output_device(int aoDevID, int aoChnID) {
    IMPAudioIOAttr attr;
    AudioOutputAttributes attrs = get_audio_attributes();

    // Set audio attributes based on the configuration or default values
    attr.bitwidth = attrs.bitwidthItem ? string_to_bitwidth(attrs.bitwidthItem->valuestring) : AUDIO_BIT_WIDTH_16;
    attr.soundmode = attrs.soundmodeItem ? string_to_soundmode(attrs.soundmodeItem->valuestring) : AUDIO_SOUND_MODE_MONO;
    attr.frmNum = attrs.frmNumItem ? attrs.frmNumItem->valueint : DEFAULT_AO_FRM_NUM;

    // Validate and set samplerate for the audio device
    attr.samplerate = attrs.samplerateItem ? attrs.samplerateItem->valueint : DEFAULT_AO_SAMPLE_RATE;
    if (!is_valid_samplerate(attr.samplerate)) {
        IMP_LOG_ERR(TAG, "Invalid samplerate value: %d. Using default value: %d.\n", attr.samplerate, DEFAULT_AO_SAMPLE_RATE);
        attr.samplerate = DEFAULT_AO_SAMPLE_RATE;
    }

    attr.numPerFrm = compute_numPerFrm(attr.samplerate);

    int chnCnt = attrs.chnCntItem ? attrs.chnCntItem->valueint : DEFAULT_AO_CHN_CNT;
    if (chnCnt > 1) {
        IMP_LOG_ERR(TAG, "chnCnt value out of range: %d. Using default value: %d.\n", chnCnt, DEFAULT_AO_CHN_CNT);
        chnCnt = DEFAULT_AO_CHN_CNT;
    }
    attr.chnCnt = chnCnt;

    // Initialize the audio device
    if (IMP_AO_SetPubAttr(aoDevID, &attr) || IMP_AO_GetPubAttr(aoDevID, &attr) ||
        IMP_AO_Enable(aoDevID) || IMP_AO_EnableChn(aoDevID, aoChnID)) {
        handle_audio_error("AO: Failed to initialize audio attributes");
        exit(EXIT_FAILURE);
    }

    // Set volume and gain for the audio device
    int vol = attrs.SetVolItem ? attrs.SetVolItem->valueint : DEFAULT_AO_CHN_VOL;
    if (vol < -30 || vol > 120) {
        IMP_LOG_ERR(TAG, "SetVol value out of range: %d. Using default value: %d.\n", vol, DEFAULT_AO_CHN_VOL);
        vol = DEFAULT_AO_CHN_VOL;
    }
    if (IMP_AO_SetVol(aoDevID, aoChnID, vol)) {
        handle_audio_error("Failed to set volume attribute");
    }

    int gain = attrs.SetGainItem ? attrs.SetGainItem->valueint : DEFAULT_AO_GAIN;
    if (gain < 0 || gain > 31) {
        IMP_LOG_ERR(TAG, "SetGain value out of range: %d. Using default value: %d.\n", gain, DEFAULT_AO_GAIN);
        gain = DEFAULT_AO_GAIN;
    }
    if (IMP_AO_SetGain(aoDevID, aoChnID, gain)) {
        handle_audio_error("Failed to set gain attribute");
    }

    // Get frame size from config and set it
    int frame_size_from_config = config_get_ao_frame_size();
    set_ao_max_frame_size(frame_size_from_config);

    // Allocate memory for audio_buffer based on the frame size
    audio_buffer = (unsigned char*) malloc(g_ao_max_frame_size);
    if (!audio_buffer) {
        // Handle memory allocation failure
        handle_audio_error("AO: Failed to allocate memory for audio_buffer");
        exit(EXIT_FAILURE);
    }

    // Debugging prints
    printf("[INFO] AO samplerate: %d\n", attr.samplerate);
    printf("[INFO] AO Volume: %d\n", vol);
    printf("[INFO] AO Gain: %d\n", gain);

}

/**
 * Cleans up resources used for audio output.
 * This primarily involves freeing the memory allocated for the audio buffer.
 */
void cleanup_audio_output() {
    if (audio_buffer) {
        free(audio_buffer);
        audio_buffer = NULL;
    }
}

/**
 * Reinitialize the audio device by first disabling it and then initializing.
 * @param aoDevID Device ID.
 * @param aoChnID Channel ID.
 */
void reinitialize_audio_output_device(int aoDevID, int aoChnID) {
    IMP_AO_DisableChn(aoDevID, aoChnID);
    IMP_AO_Disable(aoDevID);
    initialize_audio_output_device(aoDevID, aoChnID);
}

/**
 * Thread function to continuously play audio.
 * @param arg Thread arguments.
 * @return NULL.
 */
void *ao_play_thread(void *arg) {
    // Boost the thread priority for real-time audio playback
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    int aoDevID, aoChnID;
    get_audio_output_device_attributes(&aoDevID, &aoChnID);

    // Initialize the audio device for playback
    initialize_audio_output_device(aoDevID, aoChnID);

    // Continuous loop to play audio
    while (TRUE) {
        pthread_mutex_lock(&audio_buffer_lock);

        // Wait until there's audio data in the buffer
        while (audio_buffer_size == 0) {
            // Add thread termination check here
            pthread_mutex_lock(&g_stop_thread_mutex);
            if (g_stop_thread) {
                pthread_mutex_unlock(&audio_buffer_lock);
                pthread_mutex_unlock(&g_stop_thread_mutex);
                return NULL;
            }
            pthread_mutex_unlock(&g_stop_thread_mutex);

            pthread_cond_wait(&audio_data_cond, &audio_buffer_lock);
        }

        IMPAudioFrame frm = {.virAddr = (uint32_t *)audio_buffer, .len = audio_buffer_size};

        // Send the audio frame for playback
        if (IMP_AO_SendFrame(aoDevID, aoChnID, &frm, BLOCK)) {
            pthread_mutex_unlock(&audio_buffer_lock);
            handle_and_reinitialize_output(aoDevID, aoChnID, "IMP_AO_SendFrame data error");
            continue;
        }

        audio_buffer_size = 0;
        pthread_mutex_unlock(&audio_buffer_lock);
    }

    return NULL;
}

/**
 * Disables the audio channel and audio devices.
 * @return 0 on success, -1 on failure.
 */
int disable_audio_output() {
    int ret;

    int aoDevID, aoChnID;
    get_audio_output_device_attributes(&aoDevID, &aoChnID);

    // Mute the channel before we disable it
    int mute_status = 0;
    mute_audio_output_device(mute_status);

    ret = IMP_AO_DisableChn(aoDevID, aoChnID);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "Audio channel disable error\n");
        return -1;
    }

    ret = IMP_AO_Disable(aoDevID);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "Audio device disable error\n");
        return -1;
    }

    cleanup_audio_output();

    return 0;
}
