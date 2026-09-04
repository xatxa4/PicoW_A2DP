vendor/pico_audio_i2s_32b/src/pico_audio_32b/include/pico/audio.h
/*
 * Minimal copy of elehobica pico_audio header adapted for integration
 * Only the parts required by PicoW_A2DP are included.
 * Source: elehobica/pico_audio_i2s_32b (MIT/BSD as per original)
 */

#ifndef _PICO_AUDIO_H
#define _PICO_AUDIO_H

#include "pico.h"
#include "pico/util/buffer.h"
#include "hardware/sync.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AUDIO_PCM_FORMAT_S32 = 0,
    AUDIO_PCM_FORMAT_S16,
    AUDIO_PCM_FORMAT_S8,
    AUDIO_PCM_FORMAT_U32,
    AUDIO_PCM_FORMAT_U16,
    AUDIO_PCM_FORMAT_U8
} audio_pcm_format_t;

typedef enum {
    AUDIO_CHANNEL_MONO = 1,
    AUDIO_CHANNEL_STEREO = 2
} audio_channel_t;

typedef struct audio_format {
    uint32_t sample_freq;
    audio_pcm_format_t pcm_format;
    audio_channel_t channel_count;
} audio_format_t;

typedef struct audio_buffer_format {
    const audio_format_t *format;
    uint16_t sample_stride;
} audio_buffer_format_t;

typedef struct audio_buffer {
    mem_buffer_t *buffer;
    const audio_buffer_format_t *format;
    uint32_t sample_count;
    uint32_t max_sample_count;
    uint32_t user_data;
    struct audio_buffer *next;
} audio_buffer_t;

typedef struct audio_buffer_pool audio_buffer_pool_t;

#ifdef __cplusplus
}
#endif

#endif //_PICO_AUDIO_H
