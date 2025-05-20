#include <stdio.h>
#include "k_filter.h"

#define FILTER_SIZE 5

float buffer[FILTER_SIZE];

int main() {
    MovingAverageFilter ma;
    ma_init(&ma, buffer, FILTER_SIZE);

    float sample;
    for (int i = 0; i < 20; ++i) {
        sample = i + (i % 3 == 0 ? 10 : 0); // add noise every 3 samples
        float filtered = ma_update(&ma, sample);
        printf("Input: %.2f	Filtered: %.2f\n", sample, filtered);
    }

    return 0;
}
