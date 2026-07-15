#include "k_filter.h"
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
    return KT_SUMMARY();
}
