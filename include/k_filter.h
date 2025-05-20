#ifndef K_FILTER_H
#define K_FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

// Moving Average
typedef struct {
    float *buffer;
    int size;
    int index;
    float sum;
} MovingAverageFilter;

void ma_init(MovingAverageFilter *filt, float *buffer, int size);
float ma_update(MovingAverageFilter *filt, float new_sample);

// Low Pass
typedef struct {
    float alpha;
    float prev_output;
} LowPassFilter;

void lp_init(LowPassFilter *filt, float alpha);
float lp_update(LowPassFilter *filt, float new_sample);

// Median Filter
typedef struct {
    float *buffer;
    int size;
    int index;
    int count;
} MedianFilter;

void med_init(MedianFilter *filt, float *buffer, int size);
float med_update(MedianFilter *filt, float new_sample);

// Exponential Moving Average
typedef struct {
    float alpha;
    float ema_prev;
    int initialized;
} EMAFilter;

void ema_init(EMAFilter *filt, float alpha);
float ema_update(EMAFilter *filt, float new_sample);

// Kalman Filter (Basic 1D)
typedef struct {
    float estimate;
    float error_estimate;
    float error_measure;
    float kalman_gain;
} KalmanFilter;

void kalman_init(KalmanFilter *filt, float error_measure, float error_estimate, float initial_estimate);
float kalman_update(KalmanFilter *filt, float measurement);

#ifdef __cplusplus
}
#endif

#endif
