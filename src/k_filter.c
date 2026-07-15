#include "k_filter.h"

/* No <stdlib.h>, no <math.h>: the runtime is dependency-free.
 * Inputs are contractually finite (see k_filter.h); comparisons below rely on it.
 */

/* ==========================================================================
 * Moving Average
 * ========================================================================== */
kf_status_t ma_init(MovingAverageFilter *filt, kf_float_t *buffer, uint16_t size) {
    uint16_t i;
    if (filt == NULL || buffer == NULL) return KF_ERR_NULL;
    if (size == 0U) return KF_ERR_PARAM;
    filt->buffer = buffer;
    filt->size = size;
    filt->index = 0U;
    filt->count = 0U;
    filt->sum = (kf_acc_t)0;
    for (i = 0U; i < size; ++i) buffer[i] = (kf_float_t)0;
    return KF_OK;
}

kf_float_t ma_update(MovingAverageFilter *filt, kf_float_t new_sample) {
    filt->sum -= (kf_acc_t)filt->buffer[filt->index];
    filt->buffer[filt->index] = new_sample;
    filt->sum += (kf_acc_t)new_sample;
    filt->index = (uint16_t)((filt->index + 1U) % filt->size);
    if (filt->count < filt->size) filt->count++;
    return (kf_float_t)(filt->sum / (kf_acc_t)filt->count);
}

void ma_reset(MovingAverageFilter *filt) {
    uint16_t i;
    if (filt == NULL || filt->buffer == NULL) return;
    filt->index = 0U;
    filt->count = 0U;
    filt->sum = (kf_acc_t)0;
    for (i = 0U; i < filt->size; ++i) filt->buffer[i] = (kf_float_t)0;
}

kf_float_t ma_peek(const MovingAverageFilter *filt) {
    if (filt == NULL || filt->count == 0U) return (kf_float_t)0;
    return (kf_float_t)(filt->sum / (kf_acc_t)filt->count);
}

/* ==========================================================================
 * Low-Pass (1st-order IIR), seeds on first sample.
 * ========================================================================== */
kf_status_t lp_init(LowPassFilter *filt, kf_float_t alpha) {
    if (filt == NULL) return KF_ERR_NULL;
    if (alpha < (kf_float_t)0 || alpha > (kf_float_t)1) return KF_ERR_PARAM;
    filt->alpha = alpha;
    filt->prev_output = (kf_float_t)0;
    filt->initialized = 0U;
    return KF_OK;
}

kf_float_t lp_update(LowPassFilter *filt, kf_float_t new_sample) {
    if (!filt->initialized) {
        filt->prev_output = new_sample;
        filt->initialized = 1U;
    } else {
        filt->prev_output = filt->alpha * new_sample
                          + ((kf_float_t)1 - filt->alpha) * filt->prev_output;
    }
    return filt->prev_output;
}

void lp_reset(LowPassFilter *filt) {
    if (filt == NULL) return;
    filt->prev_output = (kf_float_t)0;
    filt->initialized = 0U;
}

kf_status_t lp_set_alpha(LowPassFilter *filt, kf_float_t alpha) {
    if (filt == NULL) return KF_ERR_NULL;
    if (alpha < (kf_float_t)0 || alpha > (kf_float_t)1) return KF_ERR_PARAM;
    filt->alpha = alpha;
    return KF_OK;
}

kf_float_t lp_peek(const LowPassFilter *filt) {
    if (filt == NULL) return (kf_float_t)0;
    return filt->prev_output;
}

/* ==========================================================================
 * Median: bounded, deterministic, no qsort / no VLA.
 * A fixed-size stack scratch (KF_MEDIAN_MAX_WINDOW) is insertion-sorted.
 * ========================================================================== */
kf_status_t med_init(MedianFilter *filt, kf_float_t *buffer, uint16_t size) {
    uint16_t i;
    if (filt == NULL || buffer == NULL) return KF_ERR_NULL;
    if (size == 0U || size > (uint16_t)KF_MEDIAN_MAX_WINDOW) return KF_ERR_PARAM;
    filt->buffer = buffer;
    filt->size = size;
    filt->index = 0U;
    filt->count = 0U;
    for (i = 0U; i < size; ++i) buffer[i] = (kf_float_t)0;
    return KF_OK;
}

kf_float_t med_update(MedianFilter *filt, kf_float_t new_sample) {
    kf_float_t scratch[KF_MEDIAN_MAX_WINDOW];
    uint16_t i;
    int j;
    uint16_t n;
    uint16_t mid;

    filt->buffer[filt->index] = new_sample;
    filt->index = (uint16_t)((filt->index + 1U) % filt->size);
    if (filt->count < filt->size) filt->count++;
    n = filt->count;

    /* Live values occupy buffer[0 .. count-1] during warm-up (sequential fill),
     * and all `size` slots once full. Copy, then insertion-sort ascending. */
    for (i = 0U; i < n; ++i) scratch[i] = filt->buffer[i];
    for (i = 1U; i < n; ++i) {
        kf_float_t key = scratch[i];
        for (j = (int)i - 1; j >= 0 && scratch[j] > key; --j) {
            scratch[j + 1] = scratch[j];
        }
        scratch[j + 1] = key;
    }

    mid = (uint16_t)(n / 2U);
    if ((n & 1U) != 0U) {
        return scratch[mid];
    }
    return (kf_float_t)((scratch[mid - 1U] + scratch[mid]) * (kf_float_t)0.5);
}

void med_reset(MedianFilter *filt) {
    uint16_t i;
    if (filt == NULL || filt->buffer == NULL) return;
    filt->index = 0U;
    filt->count = 0U;
    for (i = 0U; i < filt->size; ++i) filt->buffer[i] = (kf_float_t)0;
}

/* ==========================================================================
 * Exponential Moving Average (== 1st-order low-pass), seeds on first sample.
 * ========================================================================== */
kf_status_t ema_init(EMAFilter *filt, kf_float_t alpha) {
    if (filt == NULL) return KF_ERR_NULL;
    if (alpha < (kf_float_t)0 || alpha > (kf_float_t)1) return KF_ERR_PARAM;
    filt->alpha = alpha;
    filt->ema_prev = (kf_float_t)0;
    filt->initialized = 0U;
    return KF_OK;
}

kf_float_t ema_update(EMAFilter *filt, kf_float_t new_sample) {
    if (!filt->initialized) {
        filt->ema_prev = new_sample;
        filt->initialized = 1U;
    } else {
        filt->ema_prev = filt->alpha * new_sample
                       + ((kf_float_t)1 - filt->alpha) * filt->ema_prev;
    }
    return filt->ema_prev;
}

void ema_reset(EMAFilter *filt) {
    if (filt == NULL) return;
    filt->ema_prev = (kf_float_t)0;
    filt->initialized = 0U;
}

kf_status_t ema_set_alpha(EMAFilter *filt, kf_float_t alpha) {
    if (filt == NULL) return KF_ERR_NULL;
    if (alpha < (kf_float_t)0 || alpha > (kf_float_t)1) return KF_ERR_PARAM;
    filt->alpha = alpha;
    return KF_OK;
}

kf_float_t ema_peek(const EMAFilter *filt) {
    if (filt == NULL) return (kf_float_t)0;
    return filt->ema_prev;
}

/* ==========================================================================
 * Kalman Filter (scalar, 1D) with process noise Q.
 * The `error_estimate += Q` step is the missing time-update that keeps the gain
 * bounded away from 0, so the estimate keeps tracking a moving signal.
 * ========================================================================== */
kf_status_t kalman_init(KalmanFilter *filt,
                        kf_float_t error_measure,
                        kf_float_t error_estimate,
                        kf_float_t process_noise,
                        kf_float_t initial_estimate) {
    if (filt == NULL) return KF_ERR_NULL;
    if (error_measure <= (kf_float_t)0) return KF_ERR_PARAM;   /* denominator must stay > 0 */
    if (error_estimate < (kf_float_t)0 || process_noise < (kf_float_t)0) return KF_ERR_PARAM;
    filt->error_measure = error_measure;
    filt->error_estimate = error_estimate;
    filt->process_noise = process_noise;
    filt->estimate = initial_estimate;
    filt->kalman_gain = (kf_float_t)0;
    return KF_OK;
}

kf_float_t kalman_update(KalmanFilter *filt, kf_float_t measurement) {
    filt->error_estimate += filt->process_noise;  /* predict: inflate uncertainty */
    filt->kalman_gain = filt->error_estimate
                      / (filt->error_estimate + filt->error_measure);
    filt->estimate += filt->kalman_gain * (measurement - filt->estimate);
    filt->error_estimate = ((kf_float_t)1 - filt->kalman_gain) * filt->error_estimate;
    return filt->estimate;
}

void kalman_reset(KalmanFilter *filt,
                  kf_float_t error_estimate,
                  kf_float_t initial_estimate) {
    if (filt == NULL) return;
    filt->error_estimate = (error_estimate < (kf_float_t)0) ? (kf_float_t)0 : error_estimate;
    filt->estimate = initial_estimate;
    filt->kalman_gain = (kf_float_t)0;
}

kf_float_t kalman_peek(const KalmanFilter *filt) {
    if (filt == NULL) return (kf_float_t)0;
    return filt->estimate;
}
