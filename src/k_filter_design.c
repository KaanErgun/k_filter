/* OPTIONAL biquad coefficient designers (RBJ audio-EQ cookbook).
 *
 * This translation unit is NOT part of the freestanding embedded core: it needs
 * libm (sin/cos). Compile it only if you want to design biquad coefficients on
 * the target at runtime; otherwise precompute coefficients offline and feed them
 * to biquad_init(). The core src/k_filter.c never includes <math.h>.
 */
#include "k_filter.h"

#if KF_ENABLE_BIQUAD   /* nothing to design if the biquad runtime is compiled out */

#include <math.h>

#define KF_PI 3.14159265358979323846

/* Shared RBJ setup: validates params and returns w0/cos/alpha in doubles. */
static kf_status_t rbj_prep(kf_float_t fc, kf_float_t q, kf_float_t fs,
                            double *cw, double *sw, double *alpha) {
    double w0;
    if (fs <= (kf_float_t)0 || fc <= (kf_float_t)0 || q <= (kf_float_t)0) return KF_ERR_PARAM;
    if ((double)fc >= 0.5 * (double)fs) return KF_ERR_PARAM;   /* below Nyquist */
    w0 = 2.0 * KF_PI * (double)fc / (double)fs;
    *cw = cos(w0);
    *sw = sin(w0);
    *alpha = *sw / (2.0 * (double)q);
    return KF_OK;
}

static kf_status_t store(BiquadFilter *filt, double b0, double b1, double b2,
                         double a0, double a1, double a2) {
    /* Normalize so a0 == 1, then hand off to the plain initializer. */
    return biquad_init(filt,
                       (kf_float_t)(b0 / a0), (kf_float_t)(b1 / a0), (kf_float_t)(b2 / a0),
                       (kf_float_t)(a1 / a0), (kf_float_t)(a2 / a0));
}

kf_status_t biquad_design_lowpass(BiquadFilter *filt, kf_float_t fc, kf_float_t q, kf_float_t fs) {
    double cw, sw, al;
    kf_status_t s;
    if (filt == NULL) return KF_ERR_NULL;
    s = rbj_prep(fc, q, fs, &cw, &sw, &al);
    if (s != KF_OK) return s;
    return store(filt, (1.0 - cw) / 2.0, 1.0 - cw, (1.0 - cw) / 2.0,
                 1.0 + al, -2.0 * cw, 1.0 - al);
}

kf_status_t biquad_design_highpass(BiquadFilter *filt, kf_float_t fc, kf_float_t q, kf_float_t fs) {
    double cw, sw, al;
    kf_status_t s;
    if (filt == NULL) return KF_ERR_NULL;
    s = rbj_prep(fc, q, fs, &cw, &sw, &al);
    if (s != KF_OK) return s;
    return store(filt, (1.0 + cw) / 2.0, -(1.0 + cw), (1.0 + cw) / 2.0,
                 1.0 + al, -2.0 * cw, 1.0 - al);
}

kf_status_t biquad_design_bandpass(BiquadFilter *filt, kf_float_t fc, kf_float_t q, kf_float_t fs) {
    double cw, sw, al;
    kf_status_t s;
    if (filt == NULL) return KF_ERR_NULL;
    s = rbj_prep(fc, q, fs, &cw, &sw, &al);
    if (s != KF_OK) return s;
    /* constant 0 dB peak-gain band-pass */
    return store(filt, al, 0.0, -al, 1.0 + al, -2.0 * cw, 1.0 - al);
}

kf_status_t biquad_design_notch(BiquadFilter *filt, kf_float_t fc, kf_float_t q, kf_float_t fs) {
    double cw, sw, al;
    kf_status_t s;
    if (filt == NULL) return KF_ERR_NULL;
    s = rbj_prep(fc, q, fs, &cw, &sw, &al);
    if (s != KF_OK) return s;
    return store(filt, 1.0, -2.0 * cw, 1.0, 1.0 + al, -2.0 * cw, 1.0 - al);
}

#endif /* KF_ENABLE_BIQUAD */
