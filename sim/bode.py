#!/usr/bin/env python3
"""Empirical frequency response (Bode) of the frequency-shaping filters.

Drives each filter with pure sinusoids, lets it settle, and measures the output
gain (dB) and phase (degrees) via a single-bin DFT at the drive frequency. Being
empirical (not a transfer function) it also works for the nonlinear filters, with
the caveat that their gain is amplitude-dependent.

Pure-Python (stdlib only). matplotlib is used only if present, for a plot.
Usage:  python3 sim/bode.py
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from filters import MovingAverage, LowPass, EMA, Biquad  # noqa: E402

FS = 200.0
FREQS = [1.0, 2.0, 5.0, 10.0, 20.0, 40.0, 80.0]

# Fresh-instance factories (filters carry state, so rebuild per measurement).
FILTERS = {
    "MovingAvg(5)": lambda: MovingAverage(5),
    "LowPass(.1)":  lambda: LowPass(0.1),
    "EMA(.1)":      lambda: EMA(0.1),
    "Biquad LP20":  lambda: Biquad.lowpass(20.0, 0.707, FS),
}


def response(make_filter, f, fs, n_periods=30, warmup_periods=60):
    """Return (gain_dB, phase_deg) of the filter at frequency f."""
    w = 2.0 * math.pi * f / fs
    period = fs / f
    warm = int(warmup_periods * period)
    m = max(1, int(n_periods * period))
    filt = make_filter()
    for n in range(warm):
        filt.update(math.sin(w * n))
    c = 0.0
    s = 0.0
    for k in range(m):
        n = warm + k
        y = filt.update(math.sin(w * n))
        c += y * math.cos(w * n)
        s += y * math.sin(w * n)
    amp = 2.0 / m * math.sqrt(c * c + s * s)
    gain_db = 20.0 * math.log10(amp) if amp > 1e-9 else -120.0
    phase = math.degrees(math.atan2(c, s))
    return gain_db, phase


def main():
    names = list(FILTERS)
    print(f"Empirical gain (dB) at fs={FS:g} Hz  —  -3 dB marks the cutoff\n")
    header = "  f(Hz)  " + "".join(f"{n:>14}" for n in names)
    print(header)
    print("  " + "-" * (len(header) - 2))
    for f in FREQS:
        cells = []
        for n in names:
            g, _ph = response(FILTERS[n], f, FS)
            cells.append(f"{g:>14.2f}")
        print(f"  {f:>5.0f}  " + "".join(cells))

    # Optional plot if matplotlib is available; the table above is the point.
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fine = [FS * (2 ** (k / 8.0)) / 512 for k in range(0, 56)]  # log-ish sweep
        fine = [x for x in fine if 0.5 <= x < FS / 2]
        plt.figure(figsize=(8, 5))
        for n in names:
            gains = [response(FILTERS[n], f, FS)[0] for f in fine]
            plt.semilogx(fine, gains, label=n)
        plt.axhline(-3, ls="--", lw=0.8, color="gray")
        plt.xlabel("frequency (Hz)")
        plt.ylabel("gain (dB)")
        plt.title(f"k_filter empirical Bode (fs={FS:g} Hz)")
        plt.legend()
        plt.grid(True, which="both", alpha=0.3)
        plt.tight_layout()
        plt.savefig("bode.png")
        print("\nsaved bode.png")
    except ImportError:
        print("\n(matplotlib not installed — printed the table only)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
