#ifndef K_FILTER_H
#define K_FILTER_H

/*
 * k_filter - lightweight, dependency-free filter library for embedded systems.
 *
 * Design contract (v2.0):
 *   - No heap allocation. All working buffers are caller-owned (see KF_NO_HEAP).
 *   - No runtime dependencies: the shipped runtime pulls in no libc, no libm.
 *   - Deterministic, bounded worst-case timing (no VLAs, no per-sample sorting).
 *   - Every filter touches only its own struct (no globals/statics), so distinct
 *     instances are reentrant/ISR-safe; a single instance shared across an ISR and
 *     mainline still needs the caller's own guard.
 *   - Inputs are assumed finite (no NaN/Inf). Feeding a non-finite sample poisons
 *     the recursive filters permanently; reject it upstream if your source can emit one.
 *   - Filters follow the convention:  init(...) validates and returns kf_status_t;
 *     update(f, x) advances one sample and returns the filtered value.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Version -------------------------------------------------------------- */
#define K_FILTER_VERSION_MAJOR 2
#define K_FILTER_VERSION_MINOR 0
#define K_FILTER_VERSION_PATCH 0
#define K_FILTER_VERSION       "2.0.0"

/* ---- No-heap guarantee marker (grep-able for safety auditors) ------------- */
#define KF_NO_HEAP 1

/* ---- Configurable scalar type --------------------------------------------
 * Default is float (single-precision FPU friendly). Define KF_USE_DOUBLE to
 * build the whole library in double precision (host reference / long integrators).
 */
#if defined(KF_USE_DOUBLE)
typedef double kf_float_t;
#else
typedef float kf_float_t;
#endif

/* Accumulator type for the moving-average running sum. Wider than kf_float_t by
 * default to resist unbounded float drift on always-on (24/7) MCU runs.
 * Override with -DKF_ACC_TYPE=... on constrained parts that must avoid double.
 */
#ifndef KF_ACC_TYPE
#define KF_ACC_TYPE double
#endif
typedef KF_ACC_TYPE kf_acc_t;

/* Compile-time upper bound on a median window. Bounds the stack scratch used by
 * med_update so timing and stack depth stay deterministic. Override if needed.
 */
#ifndef KF_MEDIAN_MAX_WINDOW
#define KF_MEDIAN_MAX_WINDOW 31
#endif

/* ---- Status codes --------------------------------------------------------- */
typedef enum {
    KF_OK        =  0,  /* success                                */
    KF_ERR_NULL  = -1,  /* a required pointer argument was NULL    */
    KF_ERR_PARAM = -2   /* a parameter was out of its valid range  */
} kf_status_t;

/* ==========================================================================
 * Moving Average
 *   buffer: caller-owned, must hold `size` elements and outlive the filter.
 *   During warm-up (before `size` samples arrive) the average is normalized by
 *   the number of samples seen so far, so the first outputs are unbiased.
 * ========================================================================== */
typedef struct {
    kf_float_t *buffer;
    uint16_t    size;
    uint16_t    index;
    uint16_t    count;   /* samples seen, saturating at size (warm-up handling) */
    kf_acc_t    sum;     /* wide running sum */
} MovingAverageFilter;

kf_status_t ma_init(MovingAverageFilter *filt, kf_float_t *buffer, uint16_t size);
kf_float_t  ma_update(MovingAverageFilter *filt, kf_float_t new_sample);
void        ma_reset(MovingAverageFilter *filt);              /* clear state, keep buffer+size */
kf_float_t  ma_peek(const MovingAverageFilter *filt);         /* last output, no advance */

/* ==========================================================================
 * Low-Pass (1st-order IIR). Seeds on the first sample (no ramp-from-zero).
 *   alpha in [0, 1]: higher = less smoothing / lower lag.
 * ========================================================================== */
typedef struct {
    kf_float_t alpha;
    kf_float_t prev_output;
    uint8_t    initialized;
} LowPassFilter;

kf_status_t lp_init(LowPassFilter *filt, kf_float_t alpha);
kf_float_t  lp_update(LowPassFilter *filt, kf_float_t new_sample);
void        lp_reset(LowPassFilter *filt);
kf_status_t lp_set_alpha(LowPassFilter *filt, kf_float_t alpha);
kf_float_t  lp_peek(const LowPassFilter *filt);

/* ==========================================================================
 * Median (odd or even window). Good at rejecting outliers/spikes.
 *   buffer: caller-owned, must hold `size` elements; 1 <= size <= KF_MEDIAN_MAX_WINDOW.
 *   Even windows return the average of the two central order statistics.
 *   No qsort, no VLA: a bounded fixed-size stack scratch is insertion-sorted.
 * ========================================================================== */
typedef struct {
    kf_float_t *buffer;
    uint16_t    size;
    uint16_t    index;
    uint16_t    count;
} MedianFilter;

kf_status_t med_init(MedianFilter *filt, kf_float_t *buffer, uint16_t size);
kf_float_t  med_update(MedianFilter *filt, kf_float_t new_sample);
void        med_reset(MedianFilter *filt);

/* ==========================================================================
 * Exponential Moving Average. Memory-efficient; seeds on the first sample.
 *   alpha in [0, 1]. Mathematically identical to the 1st-order low-pass.
 * ========================================================================== */
typedef struct {
    kf_float_t alpha;
    kf_float_t ema_prev;
    uint8_t    initialized;
} EMAFilter;

kf_status_t ema_init(EMAFilter *filt, kf_float_t alpha);
kf_float_t  ema_update(EMAFilter *filt, kf_float_t new_sample);
void        ema_reset(EMAFilter *filt);
kf_status_t ema_set_alpha(EMAFilter *filt, kf_float_t alpha);
kf_float_t  ema_peek(const EMAFilter *filt);

/* ==========================================================================
 * Kalman Filter (scalar, 1D) with process noise Q.
 *   process_noise (Q) > 0 keeps the gain bounded away from 0 so the estimate
 *   keeps tracking a moving signal instead of freezing. Set Q small for a slow,
 *   heavily-smoothed estimate; larger to follow faster changes.
 *   Requires error_measure > 0; error_estimate >= 0; process_noise >= 0.
 * ========================================================================== */
typedef struct {
    kf_float_t estimate;
    kf_float_t error_estimate;
    kf_float_t error_measure;
    kf_float_t process_noise;  /* Q */
    kf_float_t kalman_gain;
} KalmanFilter;

kf_status_t kalman_init(KalmanFilter *filt,
                        kf_float_t error_measure,
                        kf_float_t error_estimate,
                        kf_float_t process_noise,
                        kf_float_t initial_estimate);
kf_float_t  kalman_update(KalmanFilter *filt, kf_float_t measurement);
void        kalman_reset(KalmanFilter *filt,
                         kf_float_t error_estimate,
                         kf_float_t initial_estimate);
kf_float_t  kalman_peek(const KalmanFilter *filt);

#ifdef __cplusplus
}
#endif

#endif /* K_FILTER_H */
