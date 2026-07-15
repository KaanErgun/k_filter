/* Host-side parity harness (NOT shipped to an MCU).
 * Reads whitespace-separated samples from stdin, runs them through each C filter,
 * and prints one CSV row "ma,lp,med,ema,kalman" per sample. Compiled with
 * -DKF_USE_DOUBLE so the comparison against the float64 Python mirror measures
 * ALGORITHMIC parity, not float32 rounding. Params must match sim/parity_test.py.
 */
#include <stdio.h>
#include "k_filter.h"

int main(void) {
    kf_float_t mabuf[5], medbuf[5];
    MovingAverageFilter ma;
    LowPassFilter lp;
    MedianFilter md;
    EMAFilter em;
    KalmanFilter kf;
    DCBlocker dc;
    AlphaBetaFilter ab;
    BiquadFilter bq;
    kf_float_t hbuf[5];
    HampelFilter hf;
    SlewLimiter sl;
    Deadband db;
    OneEuroFilter oe;
    double x;

    if (ma_init(&ma, mabuf, 5) != KF_OK) return 2;
    if (lp_init(&lp, (kf_float_t)0.1) != KF_OK) return 2;
    if (med_init(&md, medbuf, 5) != KF_OK) return 2;
    if (ema_init(&em, (kf_float_t)0.1) != KF_OK) return 2;
    if (kalman_init(&kf, (kf_float_t)0.25, (kf_float_t)1.0,
                    (kf_float_t)0.05, (kf_float_t)0.0) != KF_OK) return 2;
    if (dc_init(&dc, (kf_float_t)0.995) != KF_OK) return 2;
    if (ab_init_tracking(&ab, (kf_float_t)0.6, (kf_float_t)1.0, (kf_float_t)0.0) != KF_OK) return 2;
    if (biquad_design_lowpass(&bq, (kf_float_t)5.0, (kf_float_t)0.707, (kf_float_t)100.0) != KF_OK) return 2;
    if (hampel_init(&hf, hbuf, 5, (kf_float_t)3.0) != KF_OK) return 2;
    if (slew_init(&sl, (kf_float_t)0.5) != KF_OK) return 2;
    if (deadband_init(&db, (kf_float_t)0.2) != KF_OK) return 2;
    if (one_euro_init(&oe, (kf_float_t)1.0, (kf_float_t)0.5, (kf_float_t)1.0, (kf_float_t)0.05) != KF_OK) return 2;

    while (scanf("%lf", &x) == 1) {
        kf_float_t s = (kf_float_t)x;
        printf("%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,"
               "%.15g,%.15g,%.15g,%.15g\n",
               (double)ma_update(&ma, s),
               (double)lp_update(&lp, s),
               (double)med_update(&md, s),
               (double)ema_update(&em, s),
               (double)kalman_update(&kf, s),
               (double)dc_update(&dc, s),
               (double)ab_update(&ab, s),
               (double)biquad_update(&bq, s),
               (double)hampel_update(&hf, s),
               (double)slew_update(&sl, s),
               (double)deadband_update(&db, s),
               (double)one_euro_update(&oe, s));
    }
    return 0;
}
