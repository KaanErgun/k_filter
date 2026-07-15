#include "k_filter.h"
#include "k_filter_fixed.h"
#include "k_test.h"

/* ---- Moving Average ------------------------------------------------------ */
static void test_ma_warmup_and_average(void) {
    kf_float_t buf[5];
    MovingAverageFilter ma;
    KT_ASSERT_EQ_INT(ma_init(&ma, buf, 5), KF_OK);

    /* First output must equal the first sample (count-normalized warm-up),
     * NOT sample/size which the v1 code returned. */
    KT_ASSERT_NEAR(ma_update(&ma, 10.0f), 10.0f, 1e-4);
    KT_ASSERT_NEAR(ma_update(&ma, 20.0f), 15.0f, 1e-4);   /* (10+20)/2 */

    /* Fill to steady state: window of five 4.0s averages to 4.0. */
    ma_reset(&ma);
    for (int i = 0; i < 10; ++i) (void)ma_update(&ma, 4.0f);
    KT_ASSERT_NEAR(ma_peek(&ma), 4.0f, 1e-4);
}

static void test_ma_validation(void) {
    kf_float_t buf[3];
    MovingAverageFilter ma;
    KT_ASSERT_EQ_INT(ma_init(NULL, buf, 3), KF_ERR_NULL);
    KT_ASSERT_EQ_INT(ma_init(&ma, NULL, 3), KF_ERR_NULL);
    KT_ASSERT_EQ_INT(ma_init(&ma, buf, 0), KF_ERR_PARAM);
    KT_ASSERT_EQ_INT(ma_init(&ma, buf, 3), KF_OK);
}

/* ---- Low-Pass ------------------------------------------------------------ */
static void test_lp_seeds_and_converges(void) {
    LowPassFilter lp;
    KT_ASSERT_EQ_INT(lp_init(&lp, 0.1f), KF_OK);
    /* Seeds on first sample: no ramp-from-zero transient. */
    KT_ASSERT_NEAR(lp_update(&lp, 100.0f), 100.0f, 1e-4);
    KT_ASSERT_NEAR(lp_update(&lp, 0.0f), 90.0f, 1e-4);   /* 0.1*0 + 0.9*100 */
    lp_reset(&lp);
    KT_ASSERT_NEAR(lp_update(&lp, 5.0f), 5.0f, 1e-4);    /* reset re-seeds */
}

static void test_lp_validation(void) {
    LowPassFilter lp;
    KT_ASSERT_EQ_INT(lp_init(NULL, 0.1f), KF_ERR_NULL);
    KT_ASSERT_EQ_INT(lp_init(&lp, 1.5f), KF_ERR_PARAM);
    KT_ASSERT_EQ_INT(lp_init(&lp, -0.1f), KF_ERR_PARAM);
    KT_ASSERT_EQ_INT(lp_init(&lp, 0.3f), KF_OK);
    KT_ASSERT_EQ_INT(lp_set_alpha(&lp, 2.0f), KF_ERR_PARAM);
}

/* ---- Median -------------------------------------------------------------- */
static void test_med_odd_even_and_spike(void) {
    kf_float_t bo[5], be[4];
    MedianFilter mo, me;

    KT_ASSERT_EQ_INT(med_init(&mo, bo, 5), KF_OK);
    (void)med_update(&mo, 1.0f);
    (void)med_update(&mo, 2.0f);
    (void)med_update(&mo, 3.0f);
    (void)med_update(&mo, 4.0f);
    KT_ASSERT_NEAR(med_update(&mo, 5.0f), 3.0f, 1e-4);   /* odd median */

    /* Spike rejection: a lone 100 in a field of 5s stays 5. */
    med_reset(&mo);
    (void)med_update(&mo, 5.0f);
    (void)med_update(&mo, 5.0f);
    (void)med_update(&mo, 5.0f);
    (void)med_update(&mo, 100.0f);
    KT_ASSERT_NEAR(med_update(&mo, 5.0f), 5.0f, 1e-4);

    /* Even window returns the average of the two central order statistics. */
    KT_ASSERT_EQ_INT(med_init(&me, be, 4), KF_OK);
    (void)med_update(&me, 1.0f);
    (void)med_update(&me, 2.0f);
    (void)med_update(&me, 3.0f);
    KT_ASSERT_NEAR(med_update(&me, 4.0f), 2.5f, 1e-4);   /* (2+3)/2 */
}

static void test_med_warmup_and_validation(void) {
    kf_float_t buf[5];
    MedianFilter m;
    KT_ASSERT_EQ_INT(med_init(&m, buf, 5), KF_OK);
    KT_ASSERT_NEAR(med_update(&m, 7.0f), 7.0f, 1e-4);    /* median of one sample */

    KT_ASSERT_EQ_INT(med_init(&m, buf, 0), KF_ERR_PARAM);
    KT_ASSERT_EQ_INT(med_init(&m, buf, KF_MEDIAN_MAX_WINDOW + 1), KF_ERR_PARAM);
}

/* ---- EMA ----------------------------------------------------------------- */
static void test_ema_seeds_and_converges(void) {
    EMAFilter e;
    KT_ASSERT_EQ_INT(ema_init(&e, 0.5f), KF_OK);
    KT_ASSERT_NEAR(ema_update(&e, 10.0f), 10.0f, 1e-4);  /* seed */
    KT_ASSERT_NEAR(ema_update(&e, 20.0f), 15.0f, 1e-4);  /* 0.5*20 + 0.5*10 */
    KT_ASSERT_EQ_INT(ema_init(&e, 3.0f), KF_ERR_PARAM);
}

/* ---- Kalman: the headline fix — it must TRACK, not freeze. --------------- */
static void test_kalman_tracks_ramp(void) {
    KalmanFilter k;
    /* R=1, P0=1, Q=0.5 -> steady-state gain 0.5, so a unit ramp settles to a
     * constant lag d = (1-K)/K = 1.0. */
    KT_ASSERT_EQ_INT(kalman_init(&k, 1.0f, 1.0f, 0.5f, 0.0f), KF_OK);

    kf_float_t z = 0.0f, est = 0.0f;
    for (int i = 1; i <= 300; ++i) {
        z = (kf_float_t)i;              /* a rising ramp */
        est = kalman_update(&k, z);
    }
    /* With process noise the gain does NOT collapse to 0 ... */
    KT_ASSERT(k.kalman_gain > 0.4f);
    /* ... so the estimate keeps tracking with a small, BOUNDED lag (~1.0),
     * instead of freezing near the initial estimate as the v1 filter did
     * (where the gain decayed to 0 and the lag grew without bound). */
    KT_ASSERT_NEAR((double)(z - est), 1.0, 0.15);
}

static void test_kalman_validation(void) {
    KalmanFilter k;
    KT_ASSERT_EQ_INT(kalman_init(NULL, 1.0f, 1.0f, 0.1f, 0.0f), KF_ERR_NULL);
    KT_ASSERT_EQ_INT(kalman_init(&k, 0.0f, 1.0f, 0.1f, 0.0f), KF_ERR_PARAM); /* R<=0 */
    KT_ASSERT_EQ_INT(kalman_init(&k, 1.0f, -1.0f, 0.1f, 0.0f), KF_ERR_PARAM);/* P<0  */
    KT_ASSERT_EQ_INT(kalman_init(&k, 1.0f, 1.0f, -0.1f, 0.0f), KF_ERR_PARAM);/* Q<0  */
    KT_ASSERT_EQ_INT(kalman_init(&k, 1.0f, 1.0f, 0.1f, 0.0f), KF_OK);
}

/* ---- Alpha-Beta: tracks a ramp with ~zero lag (unlike the scalar Kalman). - */
static void test_alphabeta_zero_ramp_lag(void) {
    AlphaBetaFilter ab;
    KT_ASSERT_EQ_INT(ab_init_tracking(&ab, 0.5f, 1.0f, 0.0f), KF_OK);

    kf_float_t z = 0.0f, est = 0.0f;
    for (int i = 1; i <= 400; ++i) {
        z = (kf_float_t)i;
        est = ab_update(&ab, z);
    }
    /* Constant-velocity model: the velocity state converges to the ramp slope,
     * so the position lag settles to ~0 (vs the scalar Kalman's fixed lag). */
    KT_ASSERT_NEAR((double)(z - est), 0.0, 0.05);
    KT_ASSERT_NEAR((double)ab_velocity(&ab), 1.0, 0.02);
}

static void test_alphabeta_validation(void) {
    AlphaBetaFilter ab;
    KT_ASSERT_EQ_INT(ab_init(&ab, 0.0f, 0.1f, 1.0f, 0.0f), KF_ERR_PARAM); /* a<=0 */
    KT_ASSERT_EQ_INT(ab_init(&ab, 2.0f, 0.1f, 1.0f, 0.0f), KF_ERR_PARAM); /* a>=2 */
    KT_ASSERT_EQ_INT(ab_init(&ab, 0.5f, 0.1f, 0.0f, 0.0f), KF_ERR_PARAM); /* dt<=0 */
    KT_ASSERT_EQ_INT(ab_init_tracking(&ab, 1.0f, 1.0f, 0.0f), KF_ERR_PARAM); /* r>=1 */
    KT_ASSERT_EQ_INT(ab_init(&ab, 0.5f, 0.1f, 1.0f, 0.0f), KF_OK);
}

/* ---- DC blocker: removes a constant offset (drives it to 0). ------------- */
static void test_dc_blocks_constant(void) {
    DCBlocker dc;
    KT_ASSERT_EQ_INT(dc_init(&dc, 0.99f), KF_OK);
    KT_ASSERT_NEAR(dc_update(&dc, 5.0f), 0.0f, 1e-6);  /* first output seeds to 0 */
    kf_float_t y = 0.0f;
    for (int i = 0; i < 50; ++i) y = dc_update(&dc, 5.0f);
    KT_ASSERT_NEAR(y, 0.0f, 1e-6);                     /* constant fully blocked */

    KT_ASSERT_EQ_INT(dc_init(&dc, 1.0f), KF_ERR_PARAM);
    KT_ASSERT_EQ_INT(dc_init(&dc, -0.1f), KF_ERR_PARAM);
}

/* ---- Complementary: fuses toward the absolute reference. ----------------- */
static void test_complementary_converges(void) {
    ComplementaryFilter c;
    KT_ASSERT_EQ_INT(comp_init(&c, 0.98f, 0.0f), KF_OK);
    kf_float_t a = 0.0f;
    /* rate = 0, reference = 10: angle relaxes toward the reference. */
    for (int i = 0; i < 500; ++i) a = comp_update(&c, 0.0f, 10.0f, 0.01f);
    KT_ASSERT_NEAR(a, 10.0f, 0.5f);
    KT_ASSERT_EQ_INT(comp_init(&c, 1.5f, 0.0f), KF_ERR_PARAM);
}

/* ---- Biquad: unity DC gain (low-pass) and notch attenuation. ------------- */
static void test_biquad_lowpass_unity_dc(void) {
    BiquadFilter bq;
    KT_ASSERT_EQ_INT(biquad_design_lowpass(&bq, 5.0f, 0.707f, 100.0f), KF_OK);
    kf_float_t y = 0.0f;
    for (int i = 0; i < 500; ++i) y = biquad_update(&bq, 1.0f);
    KT_ASSERT_NEAR(y, 1.0f, 1e-3);   /* a low-pass passes DC with unity gain */

    /* below Nyquist is required */
    KT_ASSERT_EQ_INT(biquad_design_lowpass(&bq, 60.0f, 0.707f, 100.0f), KF_ERR_PARAM);
}

static void test_biquad_notch_attenuates(void) {
    BiquadFilter bq;
    const double fs = 1000.0, fnotch = 50.0;
    KT_ASSERT_EQ_INT(biquad_design_notch(&bq, (kf_float_t)fnotch, 5.0f, (kf_float_t)fs), KF_OK);

    double peak = 0.0;
    for (int i = 0; i < 2000; ++i) {
        kf_float_t x = (kf_float_t)sin(2.0 * 3.14159265358979 * fnotch * i / fs);
        kf_float_t y = biquad_update(&bq, x);
        if (i > 1500) {                       /* measure the settled tail */
            double ay = (y < 0) ? -(double)y : (double)y;
            if (ay > peak) peak = ay;
        }
    }
    KT_ASSERT(peak < 0.15);                   /* the notch kills the 50 Hz tone */
}

/* ---- Hampel: rejects spikes but passes clean samples untouched. --------- */
static void test_hampel_rejects_spike_passes_clean(void) {
    kf_float_t buf[5];
    HampelFilter h;
    KT_ASSERT_EQ_INT(hampel_init(&h, buf, 5, 3.0f), KF_OK);

    for (int i = 0; i < 5; ++i) (void)hampel_update(&h, 10.0f);   /* window of 10s */
    KT_ASSERT_NEAR(hampel_update(&h, 10.0f), 10.0f, 1e-4);        /* clean passes */
    KT_ASSERT_NEAR(hampel_update(&h, 50.0f), 10.0f, 1e-4);        /* spike -> median */

    /* A normal continuation of a varying signal passes through unchanged. */
    hampel_reset(&h);
    (void)hampel_update(&h, 10.0f);
    (void)hampel_update(&h, 11.0f);
    (void)hampel_update(&h, 12.0f);
    (void)hampel_update(&h, 13.0f);
    (void)hampel_update(&h, 14.0f);
    KT_ASSERT_NEAR(hampel_update(&h, 15.0f), 15.0f, 1e-4);        /* within threshold */

    KT_ASSERT_EQ_INT(hampel_init(&h, buf, 0, 3.0f), KF_ERR_PARAM);
    KT_ASSERT_EQ_INT(hampel_init(&h, buf, 5, -1.0f), KF_ERR_PARAM);
}

/* ---- Slew-rate limiter: bounds the change per sample. -------------------- */
static void test_slew_limits_step(void) {
    SlewLimiter s;
    KT_ASSERT_EQ_INT(slew_init(&s, 2.0f), KF_OK);
    KT_ASSERT_NEAR(slew_update(&s, 10.0f), 10.0f, 1e-4);   /* seed */
    KT_ASSERT_NEAR(slew_update(&s, 20.0f), 12.0f, 1e-4);   /* +10 clamped to +2 */
    KT_ASSERT_NEAR(slew_update(&s, 20.0f), 14.0f, 1e-4);
    KT_ASSERT_NEAR(slew_update(&s, 5.0f), 12.0f, 1e-4);    /* -9 clamped to -2 */
    KT_ASSERT_EQ_INT(slew_init(&s, -1.0f), KF_ERR_PARAM);
}

/* ---- Deadband: holds until the input moves beyond the threshold. --------- */
static void test_deadband_holds(void) {
    Deadband d;
    KT_ASSERT_EQ_INT(deadband_init(&d, 1.0f), KF_OK);
    KT_ASSERT_NEAR(deadband_update(&d, 10.0f), 10.0f, 1e-4);  /* seed */
    KT_ASSERT_NEAR(deadband_update(&d, 10.5f), 10.0f, 1e-4);  /* within band -> hold */
    KT_ASSERT_NEAR(deadband_update(&d, 12.0f), 12.0f, 1e-4);  /* beyond band -> snap */
    KT_ASSERT_NEAR(deadband_update(&d, 11.5f), 12.0f, 1e-4);  /* within band -> hold */
    KT_ASSERT_EQ_INT(deadband_init(&d, -1.0f), KF_ERR_PARAM);
}

/* ---- One-Euro: seeds, then converges to a constant input. ---------------- */
static void test_one_euro_converges(void) {
    OneEuroFilter e;
    KT_ASSERT_EQ_INT(one_euro_init(&e, 1.0f, 0.1f, 1.0f, 0.01f), KF_OK);
    KT_ASSERT_NEAR(one_euro_update(&e, 0.0f), 0.0f, 1e-4);   /* seed */
    kf_float_t y = 0.0f;
    for (int i = 0; i < 300; ++i) y = one_euro_update(&e, 10.0f);
    KT_ASSERT_NEAR(y, 10.0f, 0.1);                            /* tracks the level */
    KT_ASSERT_EQ_INT(one_euro_init(&e, 0.0f, 0.1f, 1.0f, 0.01f), KF_ERR_PARAM); /* min_cutoff<=0 */
    KT_ASSERT_EQ_INT(one_euro_init(&e, 1.0f, 0.1f, 1.0f, 0.0f), KF_ERR_PARAM);  /* dt<=0 */
}

/* ---- Fixed-point (integer) variants for FPU-less parts. ------------------ */
static void test_ma_fixed(void) {
    int16_t buf[5];
    MovingAverageFixed m;
    KT_ASSERT_EQ_INT(ma_fixed_init(&m, buf, 5), KF_OK);
    KT_ASSERT_EQ_INT(ma_fixed_update(&m, 10), 10);   /* warm-up: count 1 */
    KT_ASSERT_EQ_INT(ma_fixed_update(&m, 20), 15);   /* (10+20)/2 */
    ma_fixed_reset(&m);
    for (int i = 0; i < 10; ++i) (void)ma_fixed_update(&m, 4);
    KT_ASSERT_EQ_INT(ma_fixed_update(&m, 4), 4);
    KT_ASSERT_EQ_INT(ma_fixed_init(&m, buf, 0), KF_ERR_PARAM);
    KT_ASSERT_EQ_INT(ma_fixed_init(NULL, buf, 5), KF_ERR_NULL);
}

static void test_ema_fixed(void) {
    EMAFixed e;
    KT_ASSERT_EQ_INT(ema_fixed_init(&e, KF_ALPHA_Q15(0.5f)), KF_OK);
    KT_ASSERT_EQ_INT(ema_fixed_update(&e, 10), 10);  /* seed */
    KT_ASSERT_EQ_INT(ema_fixed_update(&e, 20), 15);  /* 10 + 0.5*(20-10) */
    int16_t y = 0;
    for (int i = 0; i < 40; ++i) y = ema_fixed_update(&e, 20);
    KT_ASSERT(y >= 18 && y <= 20);                    /* converges (integer truncation) */
    KT_ASSERT_EQ_INT(ema_fixed_init(&e, -1), KF_ERR_PARAM);
}

int main(void) {
    printf("k_filter %s tests\n", K_FILTER_VERSION);
    KT_RUN(test_ma_warmup_and_average);
    KT_RUN(test_ma_validation);
    KT_RUN(test_lp_seeds_and_converges);
    KT_RUN(test_lp_validation);
    KT_RUN(test_med_odd_even_and_spike);
    KT_RUN(test_med_warmup_and_validation);
    KT_RUN(test_ema_seeds_and_converges);
    KT_RUN(test_kalman_tracks_ramp);
    KT_RUN(test_kalman_validation);
    KT_RUN(test_alphabeta_zero_ramp_lag);
    KT_RUN(test_alphabeta_validation);
    KT_RUN(test_dc_blocks_constant);
    KT_RUN(test_complementary_converges);
    KT_RUN(test_biquad_lowpass_unity_dc);
    KT_RUN(test_biquad_notch_attenuates);
    KT_RUN(test_hampel_rejects_spike_passes_clean);
    KT_RUN(test_slew_limits_step);
    KT_RUN(test_deadband_holds);
    KT_RUN(test_one_euro_converges);
    KT_RUN(test_ma_fixed);
    KT_RUN(test_ema_fixed);
    return KT_SUMMARY();
}
