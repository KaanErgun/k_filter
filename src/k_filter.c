#include "k_filter.h"

/* No <stdlib.h>, no <math.h>: the runtime is dependency-free.
 * Inputs are contractually finite (see k_filter.h); comparisons below rely on it.
 */

#if KF_ENABLE_MOVING_AVERAGE
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

#endif /* KF_ENABLE_MOVING_AVERAGE */

#if KF_ENABLE_LOW_PASS
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

#endif /* KF_ENABLE_LOW_PASS */

#if KF_ENABLE_MEDIAN
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

#endif /* KF_ENABLE_MEDIAN */

#if KF_ENABLE_EMA
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

#endif /* KF_ENABLE_EMA */

#if KF_ENABLE_KALMAN
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

#endif /* KF_ENABLE_KALMAN */

#if KF_ENABLE_ALPHA_BETA
/* ==========================================================================
 * Alpha-Beta tracker (constant-velocity). Tracks a ramp with zero steady-state
 * lag because it carries a velocity state.
 * ========================================================================== */
kf_status_t ab_init(AlphaBetaFilter *filt, kf_float_t a, kf_float_t b,
                    kf_float_t dt, kf_float_t x0) {
    if (filt == NULL) return KF_ERR_NULL;
    /* Stable region (roughly): 0 < a < 2, 0 < b < 4, dt > 0. */
    if (a <= (kf_float_t)0 || a >= (kf_float_t)2) return KF_ERR_PARAM;
    if (b <= (kf_float_t)0 || b >= (kf_float_t)4) return KF_ERR_PARAM;
    if (dt <= (kf_float_t)0) return KF_ERR_PARAM;
    filt->a = a;
    filt->b = b;
    filt->dt = dt;
    filt->x = x0;
    filt->v = (kf_float_t)0;
    return KF_OK;
}

kf_status_t ab_init_tracking(AlphaBetaFilter *filt, kf_float_t r,
                             kf_float_t dt, kf_float_t x0) {
    kf_float_t one_minus_r;
    if (filt == NULL) return KF_ERR_NULL;
    if (r <= (kf_float_t)0 || r >= (kf_float_t)1) return KF_ERR_PARAM;
    /* Critically-damped gains from a single smoothing pole r in (0,1). */
    one_minus_r = (kf_float_t)1 - r;
    return ab_init(filt, (kf_float_t)1 - r * r, one_minus_r * one_minus_r, dt, x0);
}

kf_float_t ab_update(AlphaBetaFilter *filt, kf_float_t measurement) {
    kf_float_t residual;
    filt->x += filt->v * filt->dt;                 /* predict */
    residual = measurement - filt->x;              /* innovation */
    filt->x += filt->a * residual;
    filt->v += (filt->b / filt->dt) * residual;
    return filt->x;
}

void ab_reset(AlphaBetaFilter *filt, kf_float_t x0) {
    if (filt == NULL) return;
    filt->x = x0;
    filt->v = (kf_float_t)0;
}

kf_float_t ab_peek(const AlphaBetaFilter *filt) {
    if (filt == NULL) return (kf_float_t)0;
    return filt->x;
}

kf_float_t ab_velocity(const AlphaBetaFilter *filt) {
    if (filt == NULL) return (kf_float_t)0;
    return filt->v;
}

#endif /* KF_ENABLE_ALPHA_BETA */

#if KF_ENABLE_DC_BLOCKER
/* ==========================================================================
 * DC blocker / 1st-order high-pass. Seeds on the first sample.
 * ========================================================================== */
kf_status_t dc_init(DCBlocker *filt, kf_float_t r) {
    if (filt == NULL) return KF_ERR_NULL;
    if (r < (kf_float_t)0 || r >= (kf_float_t)1) return KF_ERR_PARAM;
    filt->r = r;
    filt->x_prev = (kf_float_t)0;
    filt->y_prev = (kf_float_t)0;
    filt->initialized = 0U;
    return KF_OK;
}

kf_float_t dc_update(DCBlocker *filt, kf_float_t new_sample) {
    kf_float_t y;
    if (!filt->initialized) {
        filt->x_prev = new_sample;   /* seed: first output is 0, no transient */
        filt->y_prev = (kf_float_t)0;
        filt->initialized = 1U;
        return (kf_float_t)0;
    }
    y = new_sample - filt->x_prev + filt->r * filt->y_prev;
    filt->x_prev = new_sample;
    filt->y_prev = y;
    return y;
}

void dc_reset(DCBlocker *filt) {
    if (filt == NULL) return;
    filt->x_prev = (kf_float_t)0;
    filt->y_prev = (kf_float_t)0;
    filt->initialized = 0U;
}

kf_float_t dc_peek(const DCBlocker *filt) {
    if (filt == NULL) return (kf_float_t)0;
    return filt->y_prev;
}

#endif /* KF_ENABLE_DC_BLOCKER */

#if KF_ENABLE_COMPLEMENTARY
/* ==========================================================================
 * Complementary filter (two-input sensor fusion).
 * ========================================================================== */
kf_status_t comp_init(ComplementaryFilter *filt, kf_float_t alpha, kf_float_t angle0) {
    if (filt == NULL) return KF_ERR_NULL;
    if (alpha < (kf_float_t)0 || alpha > (kf_float_t)1) return KF_ERR_PARAM;
    filt->alpha = alpha;
    filt->angle = angle0;
    return KF_OK;
}

kf_float_t comp_update(ComplementaryFilter *filt, kf_float_t rate,
                       kf_float_t reference, kf_float_t dt) {
    filt->angle = filt->alpha * (filt->angle + rate * dt)
                + ((kf_float_t)1 - filt->alpha) * reference;
    return filt->angle;
}

void comp_reset(ComplementaryFilter *filt, kf_float_t angle0) {
    if (filt == NULL) return;
    filt->angle = angle0;
}

kf_float_t comp_peek(const ComplementaryFilter *filt) {
    if (filt == NULL) return (kf_float_t)0;
    return filt->angle;
}

#endif /* KF_ENABLE_COMPLEMENTARY */

#if KF_ENABLE_BIQUAD
/* ==========================================================================
 * Biquad (Direct Form II Transposed). Runtime is libm-free; the coefficient
 * designers live in the optional src/k_filter_design.c.
 * ========================================================================== */
kf_status_t biquad_init(BiquadFilter *filt, kf_float_t b0, kf_float_t b1,
                        kf_float_t b2, kf_float_t a1, kf_float_t a2) {
    if (filt == NULL) return KF_ERR_NULL;
    filt->b0 = b0; filt->b1 = b1; filt->b2 = b2;
    filt->a1 = a1; filt->a2 = a2;
    filt->z1 = (kf_float_t)0;
    filt->z2 = (kf_float_t)0;
    return KF_OK;
}

kf_float_t biquad_update(BiquadFilter *filt, kf_float_t new_sample) {
    kf_float_t y = filt->b0 * new_sample + filt->z1;
    filt->z1 = filt->b1 * new_sample - filt->a1 * y + filt->z2;
    filt->z2 = filt->b2 * new_sample - filt->a2 * y;
    return y;
}

void biquad_reset(BiquadFilter *filt) {
    if (filt == NULL) return;
    filt->z1 = (kf_float_t)0;
    filt->z2 = (kf_float_t)0;
}
#endif /* KF_ENABLE_BIQUAD */

#if KF_ENABLE_HAMPEL
/* ==========================================================================
 * Hampel filter — robust outlier rejection (median + MAD gate).
 * ========================================================================== */
/* Median of n values via bounded insertion sort (sorts `a` in place). */
static kf_float_t kf_hampel_median(kf_float_t *a, uint16_t n) {
    uint16_t i;
    int j;
    uint16_t mid;
    for (i = 1U; i < n; ++i) {
        kf_float_t key = a[i];
        for (j = (int)i - 1; j >= 0 && a[j] > key; --j) {
            a[j + 1] = a[j];
        }
        a[j + 1] = key;
    }
    mid = (uint16_t)(n / 2U);
    if ((n & 1U) != 0U) return a[mid];
    return (kf_float_t)((a[mid - 1U] + a[mid]) * (kf_float_t)0.5);
}

kf_status_t hampel_init(HampelFilter *filt, kf_float_t *buffer, uint16_t size, kf_float_t n_sigma) {
    uint16_t i;
    if (filt == NULL || buffer == NULL) return KF_ERR_NULL;
    if (size == 0U || size > (uint16_t)KF_MEDIAN_MAX_WINDOW) return KF_ERR_PARAM;
    if (n_sigma < (kf_float_t)0) return KF_ERR_PARAM;
    filt->buffer = buffer;
    filt->size = size;
    filt->index = 0U;
    filt->count = 0U;
    filt->n_sigma = n_sigma;
    for (i = 0U; i < size; ++i) buffer[i] = (kf_float_t)0;
    return KF_OK;
}

kf_float_t hampel_update(HampelFilter *filt, kf_float_t new_sample) {
    kf_float_t win[KF_MEDIAN_MAX_WINDOW];
    kf_float_t dev[KF_MEDIAN_MAX_WINDOW];
    uint16_t i, n;
    kf_float_t m, mad, thr, d;

    filt->buffer[filt->index] = new_sample;
    filt->index = (uint16_t)((filt->index + 1U) % filt->size);
    if (filt->count < filt->size) filt->count++;
    n = filt->count;

    for (i = 0U; i < n; ++i) win[i] = filt->buffer[i];
    m = kf_hampel_median(win, n);                 /* window median */
    for (i = 0U; i < n; ++i) {
        kf_float_t diff = filt->buffer[i] - m;
        dev[i] = (diff < (kf_float_t)0) ? -diff : diff;
    }
    mad = kf_hampel_median(dev, n);               /* median abs deviation */

    thr = filt->n_sigma * (kf_float_t)1.4826 * mad;
    d = new_sample - m;
    if (d < (kf_float_t)0) d = -d;
    if (d > thr) return m;                         /* outlier -> emit median */
    return new_sample;                             /* clean -> pass through */
}

void hampel_reset(HampelFilter *filt) {
    uint16_t i;
    if (filt == NULL || filt->buffer == NULL) return;
    filt->index = 0U;
    filt->count = 0U;
    for (i = 0U; i < filt->size; ++i) filt->buffer[i] = (kf_float_t)0;
}
#endif /* KF_ENABLE_HAMPEL */

#if KF_ENABLE_SLEW
/* ==========================================================================
 * Slew-rate limiter — bounds |output change| per sample.
 * ========================================================================== */
kf_status_t slew_init(SlewLimiter *filt, kf_float_t max_step) {
    if (filt == NULL) return KF_ERR_NULL;
    if (max_step < (kf_float_t)0) return KF_ERR_PARAM;
    filt->y = (kf_float_t)0;
    filt->max_step = max_step;
    filt->initialized = 0U;
    return KF_OK;
}

kf_float_t slew_update(SlewLimiter *filt, kf_float_t new_sample) {
    kf_float_t d;
    if (!filt->initialized) {
        filt->y = new_sample;
        filt->initialized = 1U;
        return filt->y;
    }
    d = new_sample - filt->y;
    if (d > filt->max_step) d = filt->max_step;
    else if (d < -filt->max_step) d = -filt->max_step;
    filt->y += d;
    return filt->y;
}

void slew_reset(SlewLimiter *filt) {
    if (filt == NULL) return;
    filt->y = (kf_float_t)0;
    filt->initialized = 0U;
}

kf_float_t slew_peek(const SlewLimiter *filt) {
    if (filt == NULL) return (kf_float_t)0;
    return filt->y;
}
#endif /* KF_ENABLE_SLEW */

#if KF_ENABLE_DEADBAND
/* ==========================================================================
 * Deadband / hysteresis hold — holds until the input moves beyond threshold.
 * ========================================================================== */
kf_status_t deadband_init(Deadband *filt, kf_float_t threshold) {
    if (filt == NULL) return KF_ERR_NULL;
    if (threshold < (kf_float_t)0) return KF_ERR_PARAM;
    filt->y = (kf_float_t)0;
    filt->threshold = threshold;
    filt->initialized = 0U;
    return KF_OK;
}

kf_float_t deadband_update(Deadband *filt, kf_float_t new_sample) {
    kf_float_t d;
    if (!filt->initialized) {
        filt->y = new_sample;
        filt->initialized = 1U;
        return filt->y;
    }
    d = new_sample - filt->y;
    if (d < (kf_float_t)0) d = -d;
    if (d > filt->threshold) filt->y = new_sample;
    return filt->y;
}

void deadband_reset(Deadband *filt) {
    if (filt == NULL) return;
    filt->y = (kf_float_t)0;
    filt->initialized = 0U;
}

kf_float_t deadband_peek(const Deadband *filt) {
    if (filt == NULL) return (kf_float_t)0;
    return filt->y;
}
#endif /* KF_ENABLE_DEADBAND */

#if KF_ENABLE_ONE_EURO
/* ==========================================================================
 * One-Euro filter — adaptive-cutoff low-pass. libm-free (2*pi is constant).
 * ========================================================================== */
#define KF_TWO_PI ((kf_float_t)6.283185307179586)

static kf_float_t kf_one_euro_alpha(kf_float_t cutoff, kf_float_t dt) {
    /* smoothing factor of a 1st-order low-pass with time constant tau = 1/(2*pi*fc) */
    kf_float_t tau = (kf_float_t)1 / (KF_TWO_PI * cutoff);
    return dt / (dt + tau);
}

kf_status_t one_euro_init(OneEuroFilter *filt, kf_float_t min_cutoff, kf_float_t beta,
                          kf_float_t dcutoff, kf_float_t dt) {
    if (filt == NULL) return KF_ERR_NULL;
    if (min_cutoff <= (kf_float_t)0 || dcutoff <= (kf_float_t)0 || dt <= (kf_float_t)0)
        return KF_ERR_PARAM;
    if (beta < (kf_float_t)0) return KF_ERR_PARAM;
    filt->min_cutoff = min_cutoff;
    filt->beta = beta;
    filt->dcutoff = dcutoff;
    filt->dt = dt;
    filt->x_prev = (kf_float_t)0;
    filt->dx_hat = (kf_float_t)0;
    filt->x_hat = (kf_float_t)0;
    filt->initialized = 0U;
    return KF_OK;
}

kf_float_t one_euro_update(OneEuroFilter *filt, kf_float_t new_sample) {
    kf_float_t dx, a_d, adx, cutoff, a;
    if (!filt->initialized) {
        filt->x_prev = new_sample;
        filt->dx_hat = (kf_float_t)0;
        filt->x_hat = new_sample;
        filt->initialized = 1U;
        return filt->x_hat;
    }
    dx = (new_sample - filt->x_prev) / filt->dt;
    a_d = kf_one_euro_alpha(filt->dcutoff, filt->dt);
    filt->dx_hat = a_d * dx + ((kf_float_t)1 - a_d) * filt->dx_hat;
    adx = (filt->dx_hat < (kf_float_t)0) ? -filt->dx_hat : filt->dx_hat;
    cutoff = filt->min_cutoff + filt->beta * adx;
    a = kf_one_euro_alpha(cutoff, filt->dt);
    filt->x_hat = a * new_sample + ((kf_float_t)1 - a) * filt->x_hat;
    filt->x_prev = new_sample;
    return filt->x_hat;
}

void one_euro_reset(OneEuroFilter *filt) {
    if (filt == NULL) return;
    filt->x_prev = (kf_float_t)0;
    filt->dx_hat = (kf_float_t)0;
    filt->x_hat = (kf_float_t)0;
    filt->initialized = 0U;
}

kf_float_t one_euro_peek(const OneEuroFilter *filt) {
    if (filt == NULL) return (kf_float_t)0;
    return filt->x_hat;
}
#endif /* KF_ENABLE_ONE_EURO */
