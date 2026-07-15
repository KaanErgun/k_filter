#include <stdio.h>
#include "k_filter.h"

#define WINDOW 5

int main(void) {
    kf_float_t ma_buf[WINDOW];
    MovingAverageFilter ma;
    KalmanFilter kf;

    /* v2.0: init validates its arguments and returns a status. */
    if (ma_init(&ma, ma_buf, WINDOW) != KF_OK) {
        fprintf(stderr, "ma_init failed\n");
        return 1;
    }
    /* Kalman now takes a process-noise term Q (0.05 here) so it keeps TRACKING
     * a moving signal instead of freezing. */
    if (kalman_init(&kf, /*R=*/0.25f, /*P0=*/1.0f, /*Q=*/0.05f, /*x0=*/0.0f) != KF_OK) {
        fprintf(stderr, "kalman_init failed\n");
        return 1;
    }

    printf("k_filter %s\n", K_FILTER_VERSION);
    printf(" i   input     ma   kalman\n");
    for (int i = 0; i < 20; ++i) {
        /* rising signal with a spike injected every 3rd sample */
        kf_float_t sample = (kf_float_t)i + ((i % 3 == 0) ? 10.0f : 0.0f);
        kf_float_t ma_out = ma_update(&ma, sample);
        kf_float_t kf_out = kalman_update(&kf, sample);
        printf("%2d  %6.2f  %6.2f  %6.2f\n",
               i, (double)sample, (double)ma_out, (double)kf_out);
    }
    return 0;
}
