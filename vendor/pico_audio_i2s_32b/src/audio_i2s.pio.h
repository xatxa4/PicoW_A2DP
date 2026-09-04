#pragma once
/* Minimal generated PIO header placeholder for audio_i2s.pio
 * This file is a lightweight stub so the vendored audio_i2s.c can compile.
 * The real PIO program binary would normally be generated at build time by pioasm.
 * Since the vendored audio_i2s.c in this branch is a minimal stub that doesn't
 * actually invoke the PIO helper functions, these declarations are sufficient
 * to satisfy the compiler.
 */

#include "hardware/pio.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const struct pio_program audio_i2s_program;
extern const uint audio_i2s_offset_entry_point;

/* Prototype for the helper defined in the .pio %c-sdk block in the original
 * PIO file. The vendored audio_i2s.c stub does not call this, but declare it
 * so code that expects it will still compile when the real implementation is
 * used later.
 */
static inline void audio_i2s_program_init(PIO pio, uint sm, uint offset, uint data_pin, uint clock_pin_base, uint res_bits){
    (void)pio; (void)sm; (void)offset; (void)data_pin; (void)clock_pin_base; (void)res_bits;
}

#ifdef __cplusplus
}
#endif
