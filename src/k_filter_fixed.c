#include "k_filter_fixed.h"

/* Integer-only. No float, no libm, no 64-bit — cheap on FPU-less MCUs. */

static int16_t kf_sat16(int32_t v) {
    if (v > 32767) return (int16_t)32767;
    if (v < -32768) return (int16_t)(-32768);
    return (int16_t)v;
}

/* ---- Moving Average (integer) -------------------------------------------- */
kf_status_t ma_fixed_init(MovingAverageFixed *filt, int16_t *buffer, uint16_t size) {
    uint16_t i;
    if (filt == NULL || buffer == NULL) return KF_ERR_NULL;
    if (size == 0U) return KF_ERR_PARAM;
    filt->buffer = buffer;
    filt->size = size;
    filt->index = 0U;
    filt->count = 0U;
    filt->sum = 0;
    for (i = 0U; i < size; ++i) buffer[i] = 0;
    return KF_OK;
}

int16_t ma_fixed_update(MovingAverageFixed *filt, int16_t new_sample) {
    filt->sum -= (int32_t)filt->buffer[filt->index];
    filt->buffer[filt->index] = new_sample;
    filt->sum += (int32_t)new_sample;
    filt->index = (uint16_t)((filt->index + 1U) % filt->size);
    if (filt->count < filt->size) filt->count++;
    return (int16_t)(filt->sum / (int32_t)filt->count);   /* count > 0 after update */
}

void ma_fixed_reset(MovingAverageFixed *filt) {
    uint16_t i;
    if (filt == NULL || filt->buffer == NULL) return;
    filt->index = 0U;
    filt->count = 0U;
    filt->sum = 0;
    for (i = 0U; i < filt->size; ++i) filt->buffer[i] = 0;
}

/* ---- EMA / low-pass (Q15) ------------------------------------------------- */
kf_status_t ema_fixed_init(EMAFixed *filt, int16_t alpha_q15) {
    if (filt == NULL) return KF_ERR_NULL;
    if (alpha_q15 < 0) return KF_ERR_PARAM;   /* 0..32767; >1.0 in Q15 is invalid */
    filt->y = 0;
    filt->alpha_q15 = alpha_q15;
    filt->initialized = 0U;
    return KF_OK;
}

int16_t ema_fixed_update(EMAFixed *filt, int16_t new_sample) {
    int32_t diff, inc;
    if (!filt->initialized) {
        filt->y = new_sample;
        filt->initialized = 1U;
        return filt->y;
    }
    /* diff in [-65535, 65535]; alpha_q15 <= 32767 -> product fits int32.
     * Arithmetic right shift by 15 (floor) — the natural hardware requantization,
     * so this matches the Verilog/VHDL and Python fixed-point models bit-for-bit.
     * Signed >> is arithmetic on every supported target (documented assumption). */
    diff = (int32_t)new_sample - (int32_t)filt->y;
    inc = ((int32_t)filt->alpha_q15 * diff) >> 15;
    filt->y = kf_sat16((int32_t)filt->y + inc);
    return filt->y;
}

void ema_fixed_reset(EMAFixed *filt) {
    if (filt == NULL) return;
    filt->y = 0;
    filt->initialized = 0U;
}
