"""Pure-Python mirror of the C filters in src/k_filter.c.

Kept numerically faithful to the C implementation so the simulation teaches what
the embedded code actually does. A parity test (sim/parity_test.py) enforces it.
"""


class MovingAverage:
    """Count-normalized warm-up: the first outputs average only the samples seen,
    so they are unbiased (matches ma_update)."""

    def __init__(self, size):
        self.size = size
        self.buffer = [0.0] * size
        self.index = 0
        self.count = 0
        self.sum = 0.0

    def update(self, sample):
        self.sum -= self.buffer[self.index]
        self.buffer[self.index] = sample
        self.sum += sample
        self.index = (self.index + 1) % self.size
        if self.count < self.size:
            self.count += 1
        return self.sum / self.count


class LowPass:
    """1st-order IIR. Seeds on the first sample (no ramp-from-zero transient)."""

    def __init__(self, alpha):
        self.alpha = alpha
        self.prev = None

    def update(self, sample):
        if self.prev is None:
            self.prev = sample
        else:
            self.prev = self.alpha * sample + (1 - self.alpha) * self.prev
        return self.prev


class Median:
    """Even windows return the average of the two central order statistics."""

    def __init__(self, size):
        self.size = size
        self.buffer = [0.0] * size
        self.index = 0
        self.count = 0

    def update(self, sample):
        self.buffer[self.index] = sample
        self.index = (self.index + 1) % self.size
        if self.count < self.size:
            self.count += 1
        window = sorted(self.buffer[:self.count])
        n = self.count
        mid = n // 2
        if n % 2 == 1:
            return window[mid]
        return (window[mid - 1] + window[mid]) / 2.0


class EMA:
    def __init__(self, alpha):
        self.alpha = alpha
        self.prev = None

    def update(self, sample):
        if self.prev is None:
            self.prev = sample
        else:
            self.prev = self.alpha * sample + (1 - self.alpha) * self.prev
        return self.prev


class Kalman1D:
    """Scalar Kalman with process noise Q. The `error_estimate += Q` time-update
    keeps the gain bounded away from 0 so the estimate keeps tracking a moving
    signal instead of freezing (matches kalman_update)."""

    def __init__(self, error_measure, error_estimate, process_noise, initial_estimate):
        self.estimate = initial_estimate
        self.error_estimate = error_estimate
        self.error_measure = error_measure
        self.process_noise = process_noise

    def update(self, measurement):
        self.error_estimate += self.process_noise            # predict
        k = self.error_estimate / (self.error_estimate + self.error_measure)
        self.estimate = self.estimate + k * (measurement - self.estimate)
        self.error_estimate = (1 - k) * self.error_estimate
        return self.estimate
