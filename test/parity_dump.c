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
    double x;

    if (ma_init(&ma, mabuf, 5) != KF_OK) return 2;
    if (lp_init(&lp, (kf_float_t)0.1) != KF_OK) return 2;
    if (med_init(&md, medbuf, 5) != KF_OK) return 2;
    if (ema_init(&em, (kf_float_t)0.1) != KF_OK) return 2;
    if (kalman_init(&kf, (kf_float_t)0.25, (kf_float_t)1.0,
                    (kf_float_t)0.05, (kf_float_t)0.0) != KF_OK) return 2;

    while (scanf("%lf", &x) == 1) {
        kf_float_t s = (kf_float_t)x;
        printf("%.15g,%.15g,%.15g,%.15g,%.15g\n",
               (double)ma_update(&ma, s),
               (double)lp_update(&lp, s),
               (double)med_update(&md, s),
               (double)ema_update(&em, s),
               (double)kalman_update(&kf, s));
    }
    return 0;
}
