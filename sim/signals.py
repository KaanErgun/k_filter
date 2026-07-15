"""Deterministic test-signal library for the k_filter simulation.

Pure-Python (stdlib only) so it runs anywhere — no numpy required. Each generator
returns (t, clean, noisy): the time index, the ground-truth clean signal, and the
same signal plus reproducible Gaussian noise. Different signals expose different
filter behaviors (a step reveals lag/overshoot, spikes reveal why the median
exists, a chirp reveals the passband).
"""
import math
import random


def _add_noise(clean, noise_std, seed):
    rng = random.Random(seed)
    return [c + rng.gauss(0.0, noise_std) for c in clean]


def sine(n=200, freq=1.0, fs=20.0, amp=1.0, noise=0.4, seed=42):
    t = list(range(n))
    clean = [amp * math.sin(2.0 * math.pi * freq * i / fs) for i in t]
    return t, clean, _add_noise(clean, noise, seed)


def step(n=200, t0=50, amp=1.0, noise=0.3, seed=1):
    t = list(range(n))
    clean = [amp if i >= t0 else 0.0 for i in t]
    return t, clean, _add_noise(clean, noise, seed)


def ramp(n=200, slope=0.05, noise=0.3, seed=2):
    t = list(range(n))
    clean = [slope * i for i in t]
    return t, clean, _add_noise(clean, noise, seed)


def impulse(n=200, at=100, amp=5.0, noise=0.2, seed=3):
    t = list(range(n))
    clean = [amp if i == at else 0.0 for i in t]
    return t, clean, _add_noise(clean, noise, seed)


def square(n=200, period=60, amp=1.0, noise=0.3, seed=4):
    t = list(range(n))
    clean = [amp if (i % period) < (period // 2) else -amp for i in t]
    return t, clean, _add_noise(clean, noise, seed)


def chirp(n=300, f0=0.2, f1=5.0, fs=50.0, amp=1.0, noise=0.3, seed=5):
    """Linear frequency sweep f0 -> f1 (Hz). Reveals each filter's passband."""
    t = list(range(n))
    clean = []
    for i in t:
        # instantaneous phase of a linear chirp
        k = (f1 - f0) / n
        phase = 2.0 * math.pi * (f0 * i + 0.5 * k * i * i) / fs
        clean.append(amp * math.sin(phase))
    return t, clean, _add_noise(clean, noise, seed)


def inject_spikes(noisy, every=25, amp=6.0, seed=7):
    """Sparse impulsive outliers on top of an existing noisy signal (in place-safe:
    returns a new list). Great for showing off the median / Hampel filters."""
    rng = random.Random(seed)
    out = list(noisy)
    for i in range(0, len(out), every):
        if i == 0:
            continue
        out[i] += amp if rng.random() > 0.5 else -amp
    return out


def load_csv(path, column=0, has_header=False):
    """Load one column of a user's own recorded sensor trace. Returns (t, values)
    with clean unknown, so pass values as `noisy` and compare filters visually /
    by noise-reduction. Uses only the stdlib csv reader (no pandas)."""
    import csv
    values = []
    with open(path, newline="") as f:
        reader = csv.reader(f)
        rows = list(reader)
    if has_header:
        rows = rows[1:]
    for row in rows:
        if not row:
            continue
        values.append(float(row[column]))
    return list(range(len(values))), values


# Registry so drivers can select a signal by name (e.g. a --signal flag).
GENERATORS = {
    "sine": sine,
    "step": step,
    "ramp": ramp,
    "impulse": impulse,
    "square": square,
    "chirp": chirp,
}
