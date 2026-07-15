#!/usr/bin/env python3
"""Filter comparison workbench — 'which filter, which parameter?'.

Runs the smoothing filters over a chosen test signal and prints a ranked metrics
table plus a plain-language recommendation. Pure-Python (stdlib only) — no numpy,
so it runs anywhere, including CI.

Usage:
    python3 sim/compare.py                 # default: step signal
    python3 sim/compare.py --signal chirp
    python3 sim/compare.py --all           # every signal
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from filters import (MovingAverage, LowPass, Median, EMA, Kalman1D,  # noqa: E402
                     AlphaBeta)
from signals import GENERATORS  # noqa: E402
from metrics import evaluate, format_table  # noqa: E402


def build_filters():
    """Fresh filter instances (they carry state, so rebuild per signal)."""
    return {
        "MovingAvg": MovingAverage(5),
        "LowPass":   LowPass(0.1),
        "Median":    Median(5),
        "EMA":       EMA(0.1),
        "Kalman":    Kalman1D(0.25, 1.0, 0.05, 0.0),
        "AlphaBeta": AlphaBeta.tracking(0.6, 1.0, 0.0),
    }


def run_signal(name):
    t, clean, noisy = GENERATORS[name]()
    is_step = (name == "step")
    final = clean[-1] if is_step else None

    rows = []
    for fname, filt in build_filters().items():
        out = [filt.update(x) for x in noisy]
        rows.append((fname, evaluate(out, clean, noisy, is_step, final)))

    print(f"\n=== signal: {name}  (n={len(t)}) ===")
    print(format_table(rows, is_step))

    best = min(rows, key=lambda r: r[1]["rmse_lc"])
    fastest = min(rows, key=lambda r: (r[1]["lag"], r[1]["rmse_lc"]))
    print(f"\n  Best reconstruction : {best[0]} "
          f"(lag-comp RMSE {best[1]['rmse_lc']:.3f}, lag {best[1]['lag']} samples)")
    if fastest[0] != best[0]:
        print(f"  Lowest lag          : {fastest[0]} "
              f"(lag {fastest[1]['lag']} samples, RMSE {fastest[1]['rmse']:.3f}) "
              f"— pick this if delay matters more than smoothness")


def main():
    ap = argparse.ArgumentParser(description="k_filter comparison workbench")
    ap.add_argument("--signal", choices=sorted(GENERATORS), default="step")
    ap.add_argument("--all", action="store_true", help="run every signal")
    args = ap.parse_args()

    if args.all:
        for name in sorted(GENERATORS):
            run_signal(name)
    else:
        run_signal(args.signal)
    return 0


if __name__ == "__main__":
    sys.exit(main())
