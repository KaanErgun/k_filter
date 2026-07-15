# k_filter — Embedded Filtering Library with Python Simulation

**Version 2.0** &nbsp;·&nbsp; lightweight · dependency-free · deterministic

`k_filter` is a small, dependency-free filter library for embedded systems. It implements the most
common signal-processing filters used to smooth noisy sensor data — with **no heap allocation** and,
as of v2.0, **no runtime dependencies at all** (not even `<stdlib.h>`). The same filters also come as
synthesizable **Verilog and VHDL** for FPGA/ASIC, and a Python model that is **bit-exact** with the C
and HDL — so a design validated in Python behaves identically on an MCU, a CPU, or in fabric.

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
| Hampel                      | Robust outlier rejection — passes clean samples, replaces only spikes.        |
| Slew-Rate Limiter           | Bounds output change per sample — protects actuators, rejects jumps.          |
| Deadband / Hysteresis       | Holds output until input moves beyond a threshold — kills chatter.            |
| One-Euro                    | Adaptive-cutoff low-pass for jittery interactive (touch / IMU-UI) input.      |

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
├── src/k_filter_fixed.c      # Optional integer/Q15 variants for FPU-less parts
├── examples/filter_test.c    # Usage example
├── test/                     # Host-only unit tests + parity harness
│   ├── k_test.h
│   ├── test_filters.c
│   └── parity_dump.c
├── hdl/                      # FPGA/ASIC — synthesizable Verilog + VHDL
│   ├── verilog/              #   *.v modules + generic testbench
│   └── vhdl/                 #   *.vhd modules + testbenches
├── sim/                      # Python simulation + models
│   ├── filters.py            #   float mirror of the C filters
│   ├── fixed_models.py       #   integer golden models for the HDL
│   ├── simulate.py           #   6-panel visualization
│   ├── compare.py / bode.py  #   metrics workbench / frequency response
│   ├── parity_test.py        #   asserts C and Python agree
│   └── hdl_parity.py         #   asserts Python == Verilog == VHDL == C
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
make bode      # empirical gain/phase (frequency response) of the shaping filters
make hdl       # bit-exact parity: Python == Verilog == VHDL == C (needs iverilog/ghdl)
make bench     # per-filter ns/update micro-benchmark
make coverage  # line coverage of the unit tests (gcov)
make run       # run the example
make cross     # freestanding cross-compile for Cortex-M0+ (needs arm-none-eabi-gcc)
make analyze   # cppcheck static analysis (if installed)
```

Verification runs **locally** via these targets — there is no hosted CI. Before committing, a quick
`make test && make parity && make cross` covers correctness, C/Python agreement, and the
freestanding-embedded guardrail.

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

---

## 🧮 Footprint, timing & configuration

RAM per instance on the default `float32` build (from `make footprint`). MA and Median also need a
caller-owned buffer of `size` floats; every other filter is self-contained. Per-update cost is O(1)
except the median, whose bounded insertion sort over its window is the only non-constant path (still
deterministic — worst case is fixed by `KF_MEDIAN_MAX_WINDOW`).

| Filter        | struct (bytes) | caller buffer      | per-update |
|---------------|---------------:|--------------------|------------|
| Moving Average| 24             | `size` × float     | O(1)       |
| Low-Pass      | 12             | —                  | O(1)       |
| Median        | 16             | `size` × float     | O(w) bounded |
| EMA           | 12             | —                  | O(1)       |
| Kalman 1D     | 20             | —                  | O(1)       |
| Alpha-Beta    | 20             | —                  | O(1)       |
| DC Blocker    | 16             | —                  | O(1)       |
| Complementary | 8              | —                  | O(1)       |
| Biquad        | 28             | —                  | O(1)       |
| Hampel        | 24             | `size` × float     | O(w) bounded |
| Slew Limiter  | 12             | —                  | O(1)       |
| Deadband      | 12             | —                  | O(1)       |
| One-Euro      | 32             | —                  | O(1)       |

**ISR / reentrancy:** every filter touches only its own struct (no globals or statics), so **distinct
instances are reentrant and ISR-safe**. A *single* instance shared between an ISR and mainline still
needs the caller's own guard.

**Fixed-point variants (FPU-less parts).** On a Cortex-M0 / AVR without an FPU, every float op is a
soft-float call. The optional `src/k_filter_fixed.c` + `include/k_filter_fixed.h` provide integer
variants of the cheap linear filters — **no float, no libm, no 64-bit math**:

```c
#include "k_filter_fixed.h"

EMAFixed ema;
ema_fixed_init(&ema, KF_ALPHA_Q15(0.1f));   /* alpha in Q15, computed once at config time */
int16_t y = ema_fixed_update(&ema, adc_sample);
```

Add that one `.c` file only if you need it; the float core is untouched. The fixed-point low-pass is
the same filter as `EMAFixed`; Kalman/biquad stay float-only.

**Compile-time configuration:**

| Macro | Default | Effect |
|-------|---------|--------|
| `KF_ENABLE_<FILTER>` | `1` | Set to `0` to strip that filter's code from the build (e.g. `-DKF_ENABLE_KALMAN=0`). |
| `KF_USE_DOUBLE` | unset | Build the whole library in `double` precision. |
| `KF_ACC_TYPE` | `double` | Moving-average accumulator type (override to avoid `double` on FPU-less parts). |
| `KF_MEDIAN_MAX_WINDOW` | `31` | Upper bound on a median window (bounds the stack scratch; checked with a static assert). |

## 📊 Python Simulation

```bash
python3 -m venv venv && source venv/bin/activate
pip install numpy matplotlib
python sim/simulate.py        # generates simulated_filters.png (6-panel comparison)
```

The `sim/filters.py` classes mirror the C filters exactly; `make parity` proves they haven't drifted.
`make bode` prints their empirical frequency response, and `make compare` ranks them per test signal.

---

## 🔲 FPGA / ASIC (Verilog & VHDL)

The same filters are provided as synthesizable **Verilog** and **VHDL** so the same design runs on an
MCU/CPU (C) *or* on an FPGA/ASIC (HDL) — and you can validate your data **bit-for-bit against Python**
before committing to silicon.

| Module | Verilog | VHDL | Kind |
|--------|---------|------|------|
| `ema_q15` | [hdl/verilog/ema_q15.v](hdl/verilog/ema_q15.v) | [hdl/vhdl/ema_q15.vhd](hdl/vhdl/ema_q15.vhd) | Q15 EMA / 1st-order low-pass |
| `moving_avg` | [hdl/verilog/moving_avg.v](hdl/verilog/moving_avg.v) | [hdl/vhdl/moving_avg.vhd](hdl/vhdl/moving_avg.vhd) | power-of-2 boxcar |
| `dc_blocker_q15` | [hdl/verilog/dc_blocker_q15.v](hdl/verilog/dc_blocker_q15.v) | [hdl/vhdl/dc_blocker_q15.vhd](hdl/vhdl/dc_blocker_q15.vhd) | Q15 high-pass / DC blocker |

Each is a synchronous streaming block with a parameterized data `WIDTH` and coefficient:

```verilog
ema_q15 #(.WIDTH(16), .ALPHA(3277)) u_lp (   // ALPHA in Q15  (~0.1)
    .clk(clk), .rst(rst),
    .in_valid(sample_valid), .x(adc_sample),  // signed input
    .y(filtered), .out_valid(filtered_valid)   // signed output
);
```

**Bit-exact, everywhere.** All arithmetic is integer with an arithmetic right shift for the Q15
requantization, chosen so **C (`k_filter_fixed.c`), Python (`sim/fixed_models.py`), Verilog, and VHDL
produce identical outputs sample-for-sample.** Verify it yourself:

```bash
make hdl        # streams one vector through Python, Verilog (iverilog), VHDL (ghdl) and C, asserts equality
```

```
filter           samples    Verilog   VHDL    C   verdict
ema_q15              200          ✓      ✓    ✓   BIT-EXACT ✓
moving_avg           200          ✓      ✓    —   BIT-EXACT ✓
dc_blocker_q15       200          ✓      ✓    —   BIT-EXACT ✓
```

Needs [Icarus Verilog](http://iverilog.icarus.com/) and/or [GHDL](https://ghdl.github.io/ghdl/);
missing tools are skipped, not failed. So: prototype and pick parameters in Python, deploy the exact
same behavior to an MCU in C or to fabric in Verilog/VHDL.

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
