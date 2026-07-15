#!/usr/bin/env python3
"""C-vs-Python numerical parity check.

Feeds a deterministic input vector through the compiled C harness (built with
-DKF_USE_DOUBLE) and through the Python mirror in filters.py, then asserts the
outputs agree. Pure-Python (no numpy) so CI needs only python3 + a C compiler.

Usage:  python3 sim/parity_test.py <path-to-parity_dump-binary>
"""
import math
import subprocess
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from filters import (MovingAverage, LowPass, Median, EMA, Kalman1D,  # noqa: E402
                     DCBlocker, AlphaBeta, Biquad,
                     Hampel, SlewLimiter, Deadband, OneEuro)

TOL = 1e-9  # double-vs-double: this is algorithmic parity, not float32 rounding


def make_input(n=400):
    """Deterministic pseudo-noisy signal: a sine plus a repeatable sawtooth and
    a periodic outlier spike (exercises the median path). No RNG, no numpy."""
    xs = []
    for i in range(n):
        clean = math.sin(0.1 * i)
        wobble = (((i * 37) % 11) - 5) * 0.05
        spike = 6.0 if (i % 53 == 0 and i > 0) else 0.0
        xs.append(clean + wobble + spike)
    return xs


def run_c(binary, xs):
    inp = "\n".join(repr(x) for x in xs) + "\n"
    out = subprocess.run([binary], input=inp, capture_output=True, text=True, check=True)
    rows = []
    for line in out.stdout.strip().splitlines():
        rows.append([float(v) for v in line.split(",")])
    return rows


def run_py(xs):
    ma = MovingAverage(5)
    lp = LowPass(0.1)
    med = Median(5)
    ema = EMA(0.1)
    kal = Kalman1D(error_measure=0.25, error_estimate=1.0,
                   process_noise=0.05, initial_estimate=0.0)
    dc = DCBlocker(0.995)
    ab = AlphaBeta.tracking(0.6, 1.0, 0.0)
    bq = Biquad.lowpass(5.0, 0.707, 100.0)
    hp = Hampel(5, 3.0)
    sl = SlewLimiter(0.5)
    db = Deadband(0.2)
    oe = OneEuro(1.0, 0.5, 1.0, 0.05)
    rows = []
    for x in xs:
        rows.append([ma.update(x), lp.update(x), med.update(x),
                     ema.update(x), kal.update(x),
                     dc.update(x), ab.update(x), bq.update(x),
                     hp.update(x), sl.update(x), db.update(x), oe.update(x)])
    return rows


def main():
    if len(sys.argv) != 2:
        print("usage: parity_test.py <parity_dump-binary>", file=sys.stderr)
        return 2
    xs = make_input()
    c_rows = run_c(sys.argv[1], xs)
    py_rows = run_py(xs)

    if len(c_rows) != len(py_rows):
        print(f"FAIL: row count {len(c_rows)} (C) vs {len(py_rows)} (Py)")
        return 1

    names = ["MovingAverage", "LowPass", "Median", "EMA", "Kalman1D",
             "DCBlocker", "AlphaBeta", "Biquad(LP)",
             "Hampel", "SlewLimiter", "Deadband", "OneEuro"]
    ncol = len(names)
    worst = [0.0] * ncol
    fails = 0
    for r, (cr, pr) in enumerate(zip(c_rows, py_rows)):
        for j in range(ncol):
            d = abs(cr[j] - pr[j])
            worst[j] = max(worst[j], d)
            if d > TOL:
                fails += 1
                if fails <= 5:
                    print(f"  FAIL row {r} {names[j]}: C={cr[j]!r} Py={pr[j]!r} d={d:.3e}")

    for j in range(ncol):
        print(f"  {names[j]:14s} max |C-Py| = {worst[j]:.3e}")
    if fails:
        print(f"\nFAILED: {fails} mismatches (tol {TOL:g})")
        return 1
    print(f"\nPASSED: C and Python agree within {TOL:g} over {len(xs)} samples")
    return 0


if __name__ == "__main__":
    sys.exit(main())
