#include "k_filter.h"
#include <stdlib.h>

// Moving Average
void ma_init(MovingAverageFilter *filt, float *buffer, int size) {
    filt->buffer = buffer;
    filt->size = size;
    filt->index = 0;
    filt->sum = 0;
    for (int i = 0; i < size; ++i) buffer[i] = 0;
}

float ma_update(MovingAverageFilter *filt, float new_sample) {
    filt->sum -= filt->buffer[filt->index];
    filt->buffer[filt->index] = new_sample;
    filt->sum += new_sample;
    filt->index = (filt->index + 1) % filt->size;
    return filt->sum / filt->size;
}

// Low Pass
void lp_init(LowPassFilter *filt, float alpha) {
    filt->alpha = alpha;
    filt->prev_output = 0;
}

float lp_update(LowPassFilter *filt, float new_sample) {
    filt->prev_output = filt->alpha * new_sample + (1 - filt->alpha) * filt->prev_output;
    return filt->prev_output;
}

// Median Filter
static int compare_floats(const void *a, const void *b) {
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

void med_init(MedianFilter *filt, float *buffer, int size) {
    filt->buffer = buffer;
    filt->size = size;
    filt->index = 0;
    filt->count = 0;
    for (int i = 0; i < size; ++i) buffer[i] = 0;
}

float med_update(MedianFilter *filt, float new_sample) {
    filt->buffer[filt->index] = new_sample;
    filt->index = (filt->index + 1) % filt->size;
    if (filt->count < filt->size) filt->count++;

    float temp[filt->count];
    for (int i = 0; i < filt->count; ++i) temp[i] = filt->buffer[i];
    qsort(temp, filt->count, sizeof(float), compare_floats);
    return temp[filt->count / 2];
}

// EMA
void ema_init(EMAFilter *filt, float alpha) {
    filt->alpha = alpha;
    filt->ema_prev = 0;
    filt->initialized = 0;
}

float ema_update(EMAFilter *filt, float new_sample) {
    if (!filt->initialized) {
        filt->ema_prev = new_sample;
        filt->initialized = 1;
    } else {
        filt->ema_prev = filt->alpha * new_sample + (1 - filt->alpha) * filt->ema_prev;
    }
    return filt->ema_prev;
}

// Kalman
void kalman_init(KalmanFilter *filt, float error_measure, float error_estimate, float initial_estimate) {
    filt->error_measure = error_measure;
    filt->error_estimate = error_estimate;
    filt->estimate = initial_estimate;
}

float kalman_update(KalmanFilter *filt, float measurement) {
    filt->kalman_gain = filt->error_estimate / (filt->error_estimate + filt->error_measure);
    filt->estimate = filt->estimate + filt->kalman_gain * (measurement - filt->estimate);
    filt->error_estimate = (1 - filt->kalman_gain) * filt->error_estimate;
    return filt->estimate;
}
