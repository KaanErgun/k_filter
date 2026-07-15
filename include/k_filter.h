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

/* ---- Compile-time feature toggles ----------------------------------------
 * Each filter defaults to enabled. Define any of these to 0 (e.g.
 * -DKF_ENABLE_KALMAN=0) to strip that filter's implementation from the build — a
 * 16-32 KB flash part need not carry filters it never calls. The type/prototype
 * declarations stay (they cost no flash); calling a disabled filter then fails to
 * link, which is the intended early signal.
 */
#ifndef KF_ENABLE_MOVING_AVERAGE
#define KF_ENABLE_MOVING_AVERAGE 1
#endif
#ifndef KF_ENABLE_LOW_PASS
#define KF_ENABLE_LOW_PASS 1
#endif
#ifndef KF_ENABLE_MEDIAN
#define KF_ENABLE_MEDIAN 1
#endif
#ifndef KF_ENABLE_EMA
#define KF_ENABLE_EMA 1
#endif
#ifndef KF_ENABLE_KALMAN
#define KF_ENABLE_KALMAN 1
#endif
#ifndef KF_ENABLE_ALPHA_BETA
#define KF_ENABLE_ALPHA_BETA 1
#endif
#ifndef KF_ENABLE_DC_BLOCKER
#define KF_ENABLE_DC_BLOCKER 1
#endif
#ifndef KF_ENABLE_COMPLEMENTARY
#define KF_ENABLE_COMPLEMENTARY 1
#endif
#ifndef KF_ENABLE_BIQUAD
#define KF_ENABLE_BIQUAD 1
#endif
#ifndef KF_ENABLE_HAMPEL
#define KF_ENABLE_HAMPEL 1
#endif
#ifndef KF_ENABLE_SLEW
#define KF_ENABLE_SLEW 1
#endif
#ifndef KF_ENABLE_DEADBAND
#define KF_ENABLE_DEADBAND 1
#endif
#ifndef KF_ENABLE_ONE_EURO
#define KF_ENABLE_ONE_EURO 1
#endif

/* ---- Compile-time assertion (C11 _Static_assert with a C89/C99 fallback) --- */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define KF_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#define KF_STATIC_ASSERT_CAT_(a, b) a##b
#define KF_STATIC_ASSERT_CAT(a, b) KF_STATIC_ASSERT_CAT_(a, b)
#define KF_STATIC_ASSERT(cond, msg) \
    typedef char KF_STATIC_ASSERT_CAT(kf_static_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

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
KF_STATIC_ASSERT(KF_MEDIAN_MAX_WINDOW >= 1, "KF_MEDIAN_MAX_WINDOW must be >= 1");

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

/* ==========================================================================
 * Alpha-Beta tracker (constant-velocity model).
 *   The fixed-gain steady-state cousin of a 2-state Kalman: it carries a
 *   velocity state, so unlike the scalar Kalman it tracks a ramp with ZERO
 *   steady-state lag. `a` is the position gain, `b` the velocity gain, `dt`
 *   the sample interval (from your timer tick).
 *   ab_init_tracking() derives stable critically-damped gains from a single
 *   smoothing parameter r in (0, 1): larger r = smoother, slower.
 * ========================================================================== */
typedef struct {
    kf_float_t x;   /* position estimate */
    kf_float_t v;   /* velocity estimate */
    kf_float_t a;   /* position gain      */
    kf_float_t b;   /* velocity gain      */
    kf_float_t dt;  /* sample interval    */
} AlphaBetaFilter;

kf_status_t ab_init(AlphaBetaFilter *filt, kf_float_t a, kf_float_t b,
                    kf_float_t dt, kf_float_t x0);
kf_status_t ab_init_tracking(AlphaBetaFilter *filt, kf_float_t r,
                             kf_float_t dt, kf_float_t x0);
kf_float_t  ab_update(AlphaBetaFilter *filt, kf_float_t measurement);
void        ab_reset(AlphaBetaFilter *filt, kf_float_t x0);
kf_float_t  ab_peek(const AlphaBetaFilter *filt);      /* position */
kf_float_t  ab_velocity(const AlphaBetaFilter *filt);  /* estimated rate */

/* ==========================================================================
 * DC blocker / 1st-order high-pass.
 *   y[n] = x[n] - x[n-1] + r * y[n-1].  Removes constant bias and slow drift
 *   (e.g. gravity from an accelerometer, ECG/EMG baseline wander). `r` in
 *   [0, 1): closer to 1 = lower cutoff. Seeds on the first sample (no transient).
 * ========================================================================== */
typedef struct {
    kf_float_t x_prev;
    kf_float_t y_prev;
    kf_float_t r;
    uint8_t    initialized;
} DCBlocker;

kf_status_t dc_init(DCBlocker *filt, kf_float_t r);
kf_float_t  dc_update(DCBlocker *filt, kf_float_t new_sample);
void        dc_reset(DCBlocker *filt);
kf_float_t  dc_peek(const DCBlocker *filt);

/* ==========================================================================
 * Complementary filter (two-sensor fusion, e.g. gyro + accel -> angle).
 *   angle = alpha*(angle + rate*dt) + (1 - alpha)*reference.
 *   This is the ONE sanctioned exception to the single-input update convention:
 *   it fuses a fast integrated rate with a slow absolute reference.
 *   alpha in [0, 1]: closer to 1 trusts the integrated rate more.
 * ========================================================================== */
typedef struct {
    kf_float_t angle;
    kf_float_t alpha;
} ComplementaryFilter;

kf_status_t comp_init(ComplementaryFilter *filt, kf_float_t alpha, kf_float_t angle0);
kf_float_t  comp_update(ComplementaryFilter *filt, kf_float_t rate,
                        kf_float_t reference, kf_float_t dt);
void        comp_reset(ComplementaryFilter *filt, kf_float_t angle0);
kf_float_t  comp_peek(const ComplementaryFilter *filt);

/* ==========================================================================
 * Biquad — 2nd-order IIR, Direct Form II Transposed (2 state words).
 *   Sharp low/high/band-pass and, critically, a notch to kill 50/60 Hz mains
 *   hum or a known vibration line. Coefficients are normalized (a0 = 1).
 *   The runtime (biquad_update) is pure arithmetic — libm-free.
 *   Cascade several in a caller array for higher-order responses.
 *
 *   The biquad_design_* helpers below compute coefficients from (fc, Q, fs)
 *   using the RBJ cookbook. They live in the OPTIONAL src/k_filter_design.c,
 *   which needs libm (sin/cos) and is NOT part of the freestanding core — omit
 *   that file (or precompute coefficients offline) to keep an MCU build libm-free.
 * ========================================================================== */
typedef struct {
    kf_float_t b0, b1, b2;
    kf_float_t a1, a2;
    kf_float_t z1, z2;   /* state */
} BiquadFilter;

kf_status_t biquad_init(BiquadFilter *filt, kf_float_t b0, kf_float_t b1,
                        kf_float_t b2, kf_float_t a1, kf_float_t a2);
kf_float_t  biquad_update(BiquadFilter *filt, kf_float_t new_sample);
void        biquad_reset(BiquadFilter *filt);

kf_status_t biquad_design_lowpass(BiquadFilter *filt, kf_float_t fc, kf_float_t q, kf_float_t fs);
kf_status_t biquad_design_highpass(BiquadFilter *filt, kf_float_t fc, kf_float_t q, kf_float_t fs);
kf_status_t biquad_design_bandpass(BiquadFilter *filt, kf_float_t fc, kf_float_t q, kf_float_t fs);
kf_status_t biquad_design_notch(BiquadFilter *filt, kf_float_t fc, kf_float_t q, kf_float_t fs);

/* ==========================================================================
 * Hampel filter — robust outlier rejection.
 *   Over a caller-provided window it computes the median m and the MAD (median
 *   of |x_i - m|). If |x_new - m| > n_sigma * 1.4826 * MAD the sample is an
 *   outlier and the median m is emitted; otherwise x_new passes through
 *   UNCHANGED — so clean samples and edges are preserved, unlike a plain median.
 *   buffer: caller-owned, `size` elements, 1 <= size <= KF_MEDIAN_MAX_WINDOW.
 * ========================================================================== */
typedef struct {
    kf_float_t *buffer;
    uint16_t    size;
    uint16_t    index;
    uint16_t    count;
    kf_float_t  n_sigma;   /* outlier threshold in robust std-devs (e.g. 3) */
} HampelFilter;

kf_status_t hampel_init(HampelFilter *filt, kf_float_t *buffer, uint16_t size, kf_float_t n_sigma);
kf_float_t  hampel_update(HampelFilter *filt, kf_float_t new_sample);
void        hampel_reset(HampelFilter *filt);

/* ==========================================================================
 * Slew-rate limiter — bounds how far the output can move per sample.
 *   Protects actuators/setpoints and rejects one-sample jumps. Deterministic,
 *   bounded output. Seeds on the first sample.
 * ========================================================================== */
typedef struct {
    kf_float_t y;
    kf_float_t max_step;   /* maximum |change| per update, >= 0 */
    uint8_t    initialized;
} SlewLimiter;

kf_status_t slew_init(SlewLimiter *filt, kf_float_t max_step);
kf_float_t  slew_update(SlewLimiter *filt, kf_float_t new_sample);
void        slew_reset(SlewLimiter *filt);
kf_float_t  slew_peek(const SlewLimiter *filt);

/* ==========================================================================
 * Deadband / hysteresis hold — kills chatter around a value.
 *   Holds the last output until the input moves beyond `threshold`, then snaps
 *   to it. Fewer needless display updates / actuations (real power savings).
 * ========================================================================== */
typedef struct {
    kf_float_t y;
    kf_float_t threshold;  /* input must move beyond this to update, >= 0 */
    uint8_t    initialized;
} Deadband;

kf_status_t deadband_init(Deadband *filt, kf_float_t threshold);
kf_float_t  deadband_update(Deadband *filt, kf_float_t new_sample);
void        deadband_reset(Deadband *filt);
kf_float_t  deadband_peek(const Deadband *filt);

/* ==========================================================================
 * One-Euro filter — adaptive-cutoff low-pass for jittery interactive signals.
 *   Heavy smoothing when the signal is still, low lag when it moves fast — best
 *   on touch / IMU-UI / pointing input. Needs dt. libm-free (the 2*pi is a
 *   compile-time constant; no trig/exp).
 *   min_cutoff (Hz) sets the floor smoothing; beta sets how fast the cutoff
 *   opens up with speed; dcutoff (Hz) smooths the derivative estimate.
 * ========================================================================== */
typedef struct {
    kf_float_t min_cutoff;
    kf_float_t beta;
    kf_float_t dcutoff;
    kf_float_t dt;
    kf_float_t x_prev;     /* previous raw input        */
    kf_float_t dx_hat;     /* filtered derivative        */
    kf_float_t x_hat;      /* filtered output            */
    uint8_t    initialized;
} OneEuroFilter;

kf_status_t one_euro_init(OneEuroFilter *filt, kf_float_t min_cutoff, kf_float_t beta,
                          kf_float_t dcutoff, kf_float_t dt);
kf_float_t  one_euro_update(OneEuroFilter *filt, kf_float_t new_sample);
void        one_euro_reset(OneEuroFilter *filt);
kf_float_t  one_euro_peek(const OneEuroFilter *filt);

#ifdef __cplusplus
}
#endif

#endif /* K_FILTER_H */
