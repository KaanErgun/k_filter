/* Prints per-filter RAM footprint (struct size) for the current build config.
 * Host-side only. `make footprint` runs it; the README table is generated from
 * its output on the default float32 build.
 */
#include <stdio.h>
#include "k_filter.h"

int main(void) {
    printf("k_filter %s  (kf_float_t = %zu bytes)\n\n",
           K_FILTER_VERSION, sizeof(kf_float_t));
    printf("%-22s %6s   %s\n", "filter struct", "bytes", "caller buffer");
    printf("%-22s %6s   %s\n", "----------------------", "-----", "-------------");
    printf("%-22s %6zu   %s\n", "MovingAverageFilter", sizeof(MovingAverageFilter), "size * kf_float_t");
    printf("%-22s %6zu   %s\n", "LowPassFilter",       sizeof(LowPassFilter),       "none");
    printf("%-22s %6zu   %s\n", "MedianFilter",        sizeof(MedianFilter),        "size * kf_float_t");
    printf("%-22s %6zu   %s\n", "EMAFilter",           sizeof(EMAFilter),           "none");
    printf("%-22s %6zu   %s\n", "KalmanFilter",        sizeof(KalmanFilter),        "none");
    printf("%-22s %6zu   %s\n", "AlphaBetaFilter",     sizeof(AlphaBetaFilter),     "none");
    printf("%-22s %6zu   %s\n", "DCBlocker",           sizeof(DCBlocker),           "none");
    printf("%-22s %6zu   %s\n", "ComplementaryFilter", sizeof(ComplementaryFilter), "none");
    printf("%-22s %6zu   %s\n", "BiquadFilter",        sizeof(BiquadFilter),        "none");
    return 0;
}
