#ifndef K_FILTER_FIXED_H
#define K_FILTER_FIXED_H

/*
 * Optional fixed-point (integer) filter variants for FPU-less MCUs
 * (Cortex-M0, AVR, …), where every float op is an expensive soft-float call.
 *
 * Same design contract as the float core: no heap, no libm, deterministic. These
 * use only int16/int32 arithmetic — no float, no 64-bit — so they stay cheap on
 * 8/16-bit parts. Compile src/k_filter_fixed.c only if you need them.
 *
 * Sample values are int16_t (e.g. raw ADC codes). Constraints so the int32
 * intermediates never overflow:
 *   - EMA/low-pass: exact, values across the full int16 range.
 *   - Moving average: size * max|sample| must fit int32 (always true for
 *     realistic windows, e.g. size <= 65535 with 15-bit samples).
 * Results are saturated to the int16 range.
 */
#include <stdint.h>
#include "k_filter.h"   /* for kf_status_t (declarations only — no float code) */

#ifdef __cplusplus
extern "C" {
#endif

/* Convert an alpha in [0, 1] to Q15 (0..32768) for the fixed-point EMA/low-pass.
 * Host-side convenience — do it once at config time, not on the MCU. */
#define KF_ALPHA_Q15(a) ((int16_t)((a) * 32767.0f + 0.5f))

/* ==========================================================================
 * Moving Average (integer). buffer: caller-owned int16_t[size].
 * ========================================================================== */
typedef struct {
    int16_t *buffer;
    uint16_t size;
    uint16_t index;
    uint16_t count;
    int32_t  sum;
} MovingAverageFixed;

kf_status_t ma_fixed_init(MovingAverageFixed *filt, int16_t *buffer, uint16_t size);
int16_t     ma_fixed_update(MovingAverageFixed *filt, int16_t new_sample);
void        ma_fixed_reset(MovingAverageFixed *filt);

/* ==========================================================================
 * EMA / 1st-order low-pass (Q15 alpha). Mathematically the same filter as the
 * float low-pass; use it wherever you'd use lp_/ema_ on an FPU-less part.
 *   y += (alpha_q15 * (x - y)) >> 15,  with the result saturated to int16.
 * ========================================================================== */
typedef struct {
    int16_t y;
    int16_t alpha_q15;   /* 0..32767, e.g. KF_ALPHA_Q15(0.1f) */
    uint8_t initialized;
} EMAFixed;

kf_status_t ema_fixed_init(EMAFixed *filt, int16_t alpha_q15);
int16_t     ema_fixed_update(EMAFixed *filt, int16_t new_sample);
void        ema_fixed_reset(EMAFixed *filt);

#ifdef __cplusplus
}
#endif

#endif /* K_FILTER_FIXED_H */
