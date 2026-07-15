# k_filter — Embedded Filtering Library with Python Simulation

**Version 2.0** &nbsp;·&nbsp; lightweight · dependency-free · deterministic

`k_filter` is a small, dependency-free filter library for embedded systems. It implements the most
common signal-processing filters used to smooth noisy sensor data — with **no heap allocation** and,
as of v2.0, **no runtime dependencies at all** (not even `<stdlib.h>`). It ships with a Python
simulation suite that visualizes each filter and is kept numerically in lock-step with the C code.

See [ROADMAP.md](ROADMAP.md) for the v2.0 plan and what is coming next.

---

## ✨ Supported Filters

| Filter Type                 | Description                                                                 |
|-----------------------------|-----------------------------------------------------------------------------|
| Moving Average              | Averages the last N samples. Count-normalized warm-up (no startup bias).     |
| Low-Pass IIR                | 1st-order exponential smoothing; seeds on the first sample.                   |
| Median                      | Rejects outliers/spikes. No `qsort`, no VLA — bounded, deterministic timing.  |
| Exponential Moving Average  | Memory-efficient smoothing (mathematically a 1st-order low-pass).            |
| Kalman Filter (1D)          | Scalar tracker **with process noise Q** — keeps tracking a moving signal.     |
| Alpha-Beta Tracker          | Constant-velocity tracker; follows a ramp with ~zero lag (carries velocity).  |
| DC Blocker / High-Pass      | Removes constant bias & slow drift (gravity, ECG/EMG baseline wander).        |
| Complementary               | Two-sensor fusion (e.g. gyro + accel → angle); cheap IMU tilt estimate.       |
| Biquad (2nd-order IIR)      | Sharp low/high/band-pass and **50/60 Hz notch** (Direct Form II Transposed).  |

---

## 🆕 What's new in 2.0

- **Kalman actually tracks now.** v1 had no process noise, so its gain decayed to zero and the estimate
  *froze* on any moving signal. v2.0 adds `Q` (a real time-update step) so it keeps following the signal.
- **Genuinely dependency-free runtime.** The median filter no longer calls `qsort` or uses a C99
  stack VLA, so the library drops its last `#include <stdlib.h>`. Timing is now bounded and
  deterministic — safe for control loops and ISR budgets.
- **Input validation + status codes.** Every `*_init` returns a `kf_status_t` and rejects NULL
  buffers, bad sizes, and out-of-range `alpha`. The hot `update()` path stays branch-free.
- **Uniform warm-up.** All filters seed cleanly on the first sample (no ramp-from-zero, no
  divide-by-full-window bias).
- **`reset()` / `set_alpha()` / `peek()`** on the filters, plus fixed-width sized types (`uint16_t`),
  a configurable scalar type (`kf_float_t`, default `float`; `-DKF_USE_DOUBLE` for double), and a
  wide (`double`) moving-average accumulator to resist drift on always-on runs.

> Migrating from 1.x? `kalman_init` takes an extra `process_noise` (Q) argument, and the `*_init`
> functions now return `kf_status_t`. See [examples/filter_test.c](examples/filter_test.c).

---

## 📦 Project Structure

```
k_filter/
├── include/k_filter.h        # Public API (single header)
├── src/k_filter.c            # Implementation (single .c, dependency-free core)
├── src/k_filter_design.c     # Optional biquad coefficient designers (needs libm)
├── examples/filter_test.c    # Usage example
├── test/                     # Host-only unit tests + parity harness
│   ├── k_test.h
│   ├── test_filters.c
│   └── parity_dump.c
├── sim/                      # Python simulation (numpy + matplotlib)
│   ├── filters.py            #   mirror of the C filters
│   ├── simulate.py           #   6-panel visualization
│   └── parity_test.py        #   asserts C and Python agree
├── Makefile                  # host build/test/cross tooling
├── CMakeLists.txt            # optional, additive
├── ROADMAP.md
└── README.md
```

---

## 🔧 Integration Guide

The canonical path is still **copy two files**:

```
include/k_filter.h
src/k_filter.c
```

With a Makefile:

```make
CFLAGS += -Ik_filter/include
SRCS   += k_filter/src/k_filter.c
```

Or with CMake (additive convenience):

```cmake
add_subdirectory(k_filter)
target_link_libraries(app PRIVATE k_filter)
```

### Use in your code

```c
#include "k_filter.h"

float buffer[5];
MovingAverageFilter ma;

if (ma_init(&ma, buffer, 5) != KF_OK) { /* handle bad params */ }
float filtered = ma_update(&ma, raw_sensor_value);
```

Kalman with process noise (keeps tracking):

```c
KalmanFilter kf;
/* error_measure R, initial error P0, process noise Q, initial estimate x0 */
kalman_init(&kf, 0.25f, 1.0f, 0.05f, 0.0f);
float estimate = kalman_update(&kf, measurement);
```

Notch out 50 Hz mains hum with a biquad (running at 1 kHz):

```c
BiquadFilter notch;
/* fc, Q, fs — designed once; runtime is libm-free */
biquad_design_notch(&notch, 50.0f, 5.0f, 1000.0f);
float clean = biquad_update(&notch, raw);
```

> The `biquad_design_*` helpers live in the **optional** `src/k_filter_design.c` (needs libm for
> `sin`/`cos`). Add that file only if you design coefficients on the target; otherwise precompute
> them offline and pass them to `biquad_init()`. The core `src/k_filter.c` never touches libm.

> ✅ No heap allocation (grep-able `KF_NO_HEAP`). No libc/libm in the runtime core. Inputs are assumed
> finite (reject NaN/Inf upstream if your source can emit one).

---

## 🧪 Build, Test & Verify

```bash
make test      # build + run the unit tests (-Wall -Wextra -Wconversion -Werror)
make parity    # assert the C and Python implementations agree numerically
make compare   # rank the smoothing filters on every test signal (pure Python)
make run       # run the example
make cross     # freestanding cross-compile for Cortex-M0+ (needs arm-none-eabi-gcc)
make analyze   # cppcheck static analysis (if installed)
```

### Which filter should I use?

`make compare` (or `python3 sim/compare.py --signal <step|ramp|impulse|square|chirp|sine>`)
runs every smoothing filter over a test signal and prints a ranked table — RMSE, a
lag-compensated RMSE, noise/error reduction in dB, delay in samples, and step overshoot /
settling — so you can pick a filter *and* a parameter for your signal instead of guessing:

```
=== signal: step  (n=200) ===
Filter        RMSE    RMSE(lag-comp) ErrRed(dB) Lag  Over%  Settle
Kalman        0.149   0.139          5.79       1    30.5   —
MovingAvg     0.152   0.131          5.64       2    26.5   —
AlphaBeta     0.208   0.208          2.93       0    50.3   —
  Best reconstruction : MovingAvg   ·   Lowest lag : AlphaBeta
```

CI (GitHub Actions) runs the tests under gcc & clang, the C-vs-Python parity check, cppcheck, and a
freestanding `arm-none-eabi` cross-compile that fails if any forbidden dependency sneaks into the
runtime.

---

## 📊 Python Simulation

```bash
python3 -m venv venv && source venv/bin/activate
pip install numpy matplotlib
python sim/simulate.py        # generates simulated_filters.png (6-panel comparison)
```

The `sim/filters.py` classes mirror the C filters exactly; `make parity` proves they haven't drifted.

---

## 📈 Filter Use Cases

| Application          | Filter Suggestions                                     |
|----------------------|--------------------------------------------------------|
| Temperature sensor   | Moving Average or EMA                                  |
| Robot arm position   | Kalman or Low-Pass                                     |
| ECG or heart rate    | Median or Kalman                                       |
| Accelerometer data   | EMA or Low-Pass                                        |
| Real-time dashboards | EMA or Kalman                                          |

---

## 🤝 Contributing

Pull requests welcome — see [ROADMAP.md](ROADMAP.md) for what's next (Python metrics engine,
test-signal library, compile-time feature toggles, Hampel / one-euro / fixed-point filters) and the
anti-scope guardrails that keep the library lightweight and dependency-free.

## 📜 License

MIT License — free for personal and commercial use.
