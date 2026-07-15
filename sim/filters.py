"""Pure-Python mirror of the C filters in src/k_filter.c.

Kept numerically faithful to the C implementation so the simulation teaches what
the embedded code actually does. A parity test (sim/parity_test.py) enforces it.
"""
import math


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


class AlphaBeta:
    """Constant-velocity tracker: carries a velocity state, so it follows a ramp
    with ~zero steady-state lag (matches ab_update)."""

    def __init__(self, a, b, dt, x0=0.0):
        self.a, self.b, self.dt = a, b, dt
        self.x, self.v = x0, 0.0

    @classmethod
    def tracking(cls, r, dt, x0=0.0):
        """Critically-damped gains from a single smoothing pole r in (0, 1)."""
        return cls(1 - r * r, (1 - r) ** 2, dt, x0)

    def update(self, z):
        self.x += self.v * self.dt
        residual = z - self.x
        self.x += self.a * residual
        self.v += (self.b / self.dt) * residual
        return self.x


class DCBlocker:
    """1st-order high-pass: y = x - x_prev + r*y_prev. Seeds on first sample."""

    def __init__(self, r):
        self.r = r
        self.x_prev = 0.0
        self.y_prev = 0.0
        self.initialized = False

    def update(self, x):
        if not self.initialized:
            self.x_prev = x
            self.y_prev = 0.0
            self.initialized = True
            return 0.0
        y = x - self.x_prev + self.r * self.y_prev
        self.x_prev = x
        self.y_prev = y
        return y


class Complementary:
    """Two-input fusion: angle = alpha*(angle + rate*dt) + (1-alpha)*reference."""

    def __init__(self, alpha, angle0=0.0):
        self.alpha = alpha
        self.angle = angle0

    def update(self, rate, reference, dt):
        self.angle = self.alpha * (self.angle + rate * dt) + (1 - self.alpha) * reference
        return self.angle


class Biquad:
    """2nd-order IIR (Direct Form II Transposed). Coefficients normalized a0=1."""

    def __init__(self, b0, b1, b2, a1, a2):
        self.b0, self.b1, self.b2 = b0, b1, b2
        self.a1, self.a2 = a1, a2
        self.z1 = 0.0
        self.z2 = 0.0

    def update(self, x):
        y = self.b0 * x + self.z1
        self.z1 = self.b1 * x - self.a1 * y + self.z2
        self.z2 = self.b2 * x - self.a2 * y
        return y

    @staticmethod
    def _rbj(fc, q, fs):
        w0 = 2.0 * math.pi * fc / fs
        return math.cos(w0), math.sin(w0), math.sin(w0) / (2.0 * q)

    @classmethod
    def lowpass(cls, fc, q, fs):
        cw, _sw, al = cls._rbj(fc, q, fs)
        a0 = 1.0 + al
        return cls((1 - cw) / 2 / a0, (1 - cw) / a0, (1 - cw) / 2 / a0,
                   (-2 * cw) / a0, (1 - al) / a0)

    @classmethod
    def notch(cls, fc, q, fs):
        cw, _sw, al = cls._rbj(fc, q, fs)
        a0 = 1.0 + al
        return cls(1 / a0, (-2 * cw) / a0, 1 / a0, (-2 * cw) / a0, (1 - al) / a0)
