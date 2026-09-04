#pragma once
#include "pico/audio.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct audio_i2s_config {
    uint8_t data_pin;
    uint8_t clock_pin_base;
    uint8_t dma_channel0;
    uint8_t dma_channel1;
    uint8_t pio_sm;
} audio_i2s_config_t;

/* Setup: pass input/output formats and the config.
 * Returns the actual output format used (or NULL on error).
 */
const audio_format_t *audio_i2s_setup(const audio_format_t *i2s_input_audio_format, const audio_format_t *i2s_output_audio_format, const audio_i2s_config_t *config);

/* Enable or disable I2S streaming */
void audio_i2s_set_enabled(bool enabled);

/* Connect a producer pool to the I2S consumer */
bool audio_i2s_connect(audio_buffer_pool_t *producer);

/* Optional path for 8-bit producers */
bool audio_i2s_connect_s8(audio_buffer_pool_t *producer);

#ifdef __cplusplus
}
#endif
