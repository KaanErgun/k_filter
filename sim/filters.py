import numpy as np

class MovingAverage:
    def __init__(self, size):
        self.size = size
        self.buffer = [0.0] * size
        self.index = 0
        self.sum = 0.0

    def update(self, sample):
        self.sum -= self.buffer[self.index]
        self.buffer[self.index] = sample
        self.sum += sample
        self.index = (self.index + 1) % self.size
        return self.sum / self.size

class LowPass:
    def __init__(self, alpha):
        self.alpha = alpha
        self.prev = 0.0

    def update(self, sample):
        self.prev = self.alpha * sample + (1 - self.alpha) * self.prev
        return self.prev

class Median:
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
        sorted_buf = sorted(self.buffer[:self.count])
        return sorted_buf[self.count // 2]

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
    def __init__(self, error_measure, error_estimate, initial_estimate):
        self.estimate = initial_estimate
        self.error_estimate = error_estimate
        self.error_measure = error_measure

    def update(self, measurement):
        kalman_gain = self.error_estimate / (self.error_estimate + self.error_measure)
        self.estimate = self.estimate + kalman_gain * (measurement - self.estimate)
        self.error_estimate = (1 - kalman_gain) * self.error_estimate
        return self.estimate
