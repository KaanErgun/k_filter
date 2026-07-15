/* Per-filter micro-benchmark: nanoseconds per update() on the host.
 * Host-side only; quantifies the "tiny and fast" claim. `make bench` runs it.
 * Not a portability guarantee — MCU cycle counts differ — but a useful relative
 * ranking (e.g. the bounded median vs the O(1) IIRs).
 */
#include <stdio.h>
#include <time.h>
#include "k_filter.h"

#define N 3000000L

static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

/* varying, libm-free input to defeat constant folding */
#define INPUT(i) ((kf_float_t)((i) & 255) * (kf_float_t)0.01)

#define BENCH(label, decls, init_expr, update_expr) do {          \
    decls;                                                        \
    volatile kf_float_t sink = (kf_float_t)0;                     \
    kf_float_t acc = (kf_float_t)0;                               \
    long i;                                                       \
    (void)(init_expr);                                           \
    double t0 = now_ns();                                        \
    for (i = 0; i < N; ++i) { kf_float_t s = INPUT(i); acc += (update_expr); } \
    double t1 = now_ns();                                        \
    sink = acc;                                                  \
    (void)sink;                                                 \
    printf("%-16s %7.2f ns/update\n", label, (t1 - t0) / (double)N); \
} while (0)

int main(void) {
    static kf_float_t mabuf[8], medbuf[8], hbuf[8];
    printf("k_filter %s  micro-benchmark  (%ld iterations each)\n\n", K_FILTER_VERSION, N);

    BENCH("MovingAverage", MovingAverageFilter f, ma_init(&f, mabuf, 8), ma_update(&f, s));
    BENCH("LowPass",       LowPassFilter f,       lp_init(&f, 0.1f),     lp_update(&f, s));
    BENCH("Median",        MedianFilter f,        med_init(&f, medbuf, 8), med_update(&f, s));
    BENCH("EMA",           EMAFilter f,           ema_init(&f, 0.1f),    ema_update(&f, s));
    BENCH("Kalman",        KalmanFilter f,        kalman_init(&f, 0.25f, 1.0f, 0.05f, 0.0f), kalman_update(&f, s));
    BENCH("AlphaBeta",     AlphaBetaFilter f,     ab_init_tracking(&f, 0.6f, 1.0f, 0.0f), ab_update(&f, s));
    BENCH("DCBlocker",     DCBlocker f,           dc_init(&f, 0.995f),   dc_update(&f, s));
    BENCH("Complementary", ComplementaryFilter f, comp_init(&f, 0.98f, 0.0f), comp_update(&f, s, 0.0f, 0.01f));
    BENCH("Biquad",        BiquadFilter f,        biquad_design_lowpass(&f, 5.0f, 0.707f, 100.0f), biquad_update(&f, s));
    BENCH("Hampel",        HampelFilter f,        hampel_init(&f, hbuf, 8, 3.0f), hampel_update(&f, s));
    BENCH("SlewLimiter",   SlewLimiter f,         slew_init(&f, 0.5f),   slew_update(&f, s));
    BENCH("Deadband",      Deadband f,            deadband_init(&f, 0.2f), deadband_update(&f, s));
    BENCH("OneEuro",       OneEuroFilter f,       one_euro_init(&f, 1.0f, 0.5f, 1.0f, 0.05f), one_euro_update(&f, s));
    return 0;
}
