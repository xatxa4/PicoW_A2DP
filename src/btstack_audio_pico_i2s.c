/*
 * Copyright (C) 2022 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "btstack_audio_pico.c"

/*
 *  btstack_audio_pico.c
 *
 *  Implementation of btstack_audio.h using pico_i2s
 *
 */

#include "btstack_config.h"

#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_run_loop.h"

#include <stddef.h>
#include <stdio.h>
#include <hardware/dma.h>

#include "pico/audio_i2s.h"

#define DRIVER_POLL_INTERVAL_MS   5
#define SAMPLES_PER_BUFFER      512

// client
static void (*playback_callback)(int16_t * buffer, uint16_t num_samples);

// timer to fill output ring buffer
static btstack_timer_source_t  driver_timer_sink;


static bool btstack_audio_pico_sink_active;

// from pico-playground/audio/sine_wave/sine_wave.c

static audio_format_t        btstack_audio_pico_audio_format;
static audio_buffer_format_t btstack_audio_pico_producer_format;
static audio_buffer_pool_t * btstack_audio_pico_audio_buffer_pool;
static uint8_t               btstack_audio_pico_channel_count;

#ifndef S16_TO_S32_SHIFT
#define S16_TO_S32_SHIFT 16
#endif

static audio_buffer_pool_t *init_audio(uint32_t sample_frequency, uint8_t channel_count) {

    // num channels requested by application
    btstack_audio_pico_channel_count = channel_count;

    // request S32 stereo output
    btstack_audio_pico_audio_format.pcm_format = AUDIO_PCM_FORMAT_S32;
    btstack_audio_pico_audio_format.sample_freq = sample_frequency;
    btstack_audio_pico_audio_format.channel_count = AUDIO_CHANNEL_STEREO;

    btstack_audio_pico_producer_format.format = &btstack_audio_pico_audio_format;
    btstack_audio_pico_producer_format.sample_stride = 4 * btstack_audio_pico_audio_format.channel_count; // 8 bytes per frame

    audio_buffer_pool_t * producer_pool = audio_new_producer_pool(&btstack_audio_pico_producer_format, 2, SAMPLES_PER_BUFFER); // todo correct size

    audio_i2s_config_t config;
    config.data_pin       = PICO_AUDIO_I2S_DATA_PIN;
    config.clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE;  // BCK, LRCK = BCK+1
    // select two free dma channels and unclaim them so audio_i2s_setup can claim
    int dma_ch0 = dma_claim_unused_channel(true);
    int dma_ch1 = dma_claim_unused_channel(true);
    dma_channel_unclaim(dma_ch0);
    dma_channel_unclaim(dma_ch1);
    config.dma_channel0 = (uint8_t)dma_ch0;
    config.dma_channel1 = (uint8_t)dma_ch1;
    config.pio_sm         = 0;

    const audio_format_t * output_format = audio_i2s_setup(&btstack_audio_pico_audio_format, &btstack_audio_pico_audio_format, &config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }

    bool ok = audio_i2s_connect(producer_pool);
    assert(ok);
    (void)ok;

    return producer_pool;
}

static void btstack_audio_pico_sink_fill_buffers(void){
    while (true){
        audio_buffer_t * audio_buffer = take_audio_buffer(btstack_audio_pico_audio_buffer_pool, false);
        if (audio_buffer == NULL){
            break;
        }

        int frames = audio_buffer->max_sample_count; // frames per channel

        static int16_t tmp16[SAMPLES_PER_BUFFER * 2]; // stereo temp buffer

        // render 16-bit PCM from existing pipeline into tmp16 (interleaved L,R,L,R...)
        (*playback_callback)(tmp16, frames);

        // if client requested mono input, duplicate into interleaved stereo before conversion
        if (btstack_audio_pico_channel_count == 1){
            for (int i = frames - 1; i >= 0; --i){
                tmp16[2*i  ] = tmp16[i];
                tmp16[2*i+1] = tmp16[i];
            }
        }

        int total_samples = frames * 2; // stereo

        int32_t * dst32 = (int32_t *) audio_buffer->buffer->bytes;
        for (int i = 0; i < total_samples; ++i){
            dst32[i] = ((int32_t) tmp16[i]) << S16_TO_S32_SHIFT;
        }

        audio_buffer->sample_count = frames; // frames per channel
        give_audio_buffer(btstack_audio_pico_audio_buffer_pool, audio_buffer);
    }
}

static void driver_timer_handler_sink(btstack_timer_source_t * ts){

    // refill
    btstack_audio_pico_sink_fill_buffers();

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static int btstack_audio_pico_sink_init(
    uint8_t channels,
    uint32_t samplerate, 
    void (*playback)(int16_t * buffer, uint16_t num_samples)
){
    btstack_assert(playback != NULL);
    btstack_assert(channels != 0);

    playback_callback  = playback;

    btstack_audio_pico_audio_buffer_pool = init_audio(samplerate, channels);

    return 0;
}

static void btstack_audio_pico_sink_set_volume(uint8_t volume){
    // printf("btstack_audio_pico_sink_set_volume: %d\n", volume);
}

static void btstack_audio_pico_sink_start_stream(void){

    // pre-fill HAL buffers
    btstack_audio_pico_sink_fill_buffers();

    // start timer
    btstack_run_loop_set_timer_handler(&driver_timer_sink, &driver_timer_handler_sink);
    btstack_run_loop_set_timer(&driver_timer_sink, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&driver_timer_sink);

    // state
    btstack_audio_pico_sink_active = true;

    audio_i2s_set_enabled(true);
}

static void btstack_audio_pico_sink_stop_stream(void){

    audio_i2s_set_enabled(false);

    // stop timer
    btstack_run_loop_remove_timer(&driver_timer_sink);
    // state
    btstack_audio_pico_sink_active = false;
}

static void btstack_audio_pico_sink_close(void){
    // stop stream if needed
    if (btstack_audio_pico_sink_active){
        btstack_audio_pico_sink_stop_stream();
    }
}

static const btstack_audio_sink_t btstack_audio_pico_sink = {
    .init = &btstack_audio_pico_sink_init,
    .set_volume = &btstack_audio_pico_sink_set_volume,
    .start_stream = &btstack_audio_pico_sink_start_stream,
    .stop_stream = &btstack_audio_pico_sink_stop_stream,
    .close = &btstack_audio_pico_sink_close,
};

const btstack_audio_sink_t * btstack_audio_pico_sink_get_instance(void){
    return &btstack_audio_pico_sink;
}
