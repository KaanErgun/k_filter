# k_filter v2.0 Roadmap

> **Guiding principle:** *"Prove and defend the identity, then earn new capability on top of it."*
> v2.0 makes the tagline literally true — genuinely **dependency-free** (drop the last `<stdlib.h>`),
> genuinely **deterministic** (no VLA, no per-sample `qsort`, bounded worst-case timing), and genuinely
> a **tracking** filter suite (fix the Kalman freeze). First repair the correctness defects that
> contradict the core purpose, then add high-value new filters, then turn the Python sim from one
> picture into a quantitative decision workbench.

The non-negotiable identity every change must honor: **no heap, no runtime dependencies in the
embedded core, tiny footprint, deterministic timing, `update(f, x) -> float` convention.**

---

## Tier 0 — Correctness & foundation (the "must" tier) ✅ DONE

These repair defects in the existing 5 filters and lay the v2.0 groundwork. Ordered by dependency.

- [x] **Fix the Kalman freeze** — added process noise `Q` + a real time-update step
      (`error_estimate += Q` before computing gain). Verified: it now tracks a ramp with bounded
      lag instead of freezing (`test_kalman_tracks_ramp`).
- [x] **Deterministic median** — removed the per-sample `qsort`, the C99 stack **VLA**, and the sole
      `#include <stdlib.h>`. Now a bounded fixed-size stack scratch is insertion-sorted; MISRA/MSVC-safe.
      *(True O(w) streaming median remains a Tier 2 refinement.)*
- [x] **Input validation + `kf_status_t`** — `*_init` returns a status; rejects NULL buffers,
      `size == 0`, `error_measure <= 0`, out-of-range `alpha`. Hot `update()` path stays branch-free.
- [x] **Uniform warm-up contract** — seed-on-first-sample everywhere. MA divides by `min(count, size)`;
      LowPass seeds like EMA instead of ramping from 0.
- [x] **Numerical hardening** — wide (`double`, overridable `kf_acc_t`) MA accumulator; documented
      non-finite (NaN/Inf) input policy; average-of-two-middles for even-count median.
- [x] **`reset()` / `set_alpha()` / `peek()` API + sized types + header contract docs** —
      clear runtime state without re-supplying the buffer; `uint16_t` sizes; documented preconditions.
- [x] **Unit-test harness** — `test/k_test.h` (stdlib-only) + `test/test_filters.c` (38 checks),
      pinning both the fixes and the intentional quirks.
- [x] **Build system** — `Makefile` (build / test / parity / cross / analyze) + optional `CMakeLists.txt`.
      The "copy two files" integration story stays canonical.
- [x] **CI** — GitHub Actions: gcc+clang `-Wall -Wextra -Wconversion -Werror`, cppcheck, C-vs-Python
      parity, and an `arm-none-eabi` freestanding cross-compile + forbidden-`#include` grep guardrail.

Also delivered alongside Tier 0: **`kf_float_t` configurable scalar type** and the **C-vs-Python
parity harness** (both were Tier 1 — pulled forward because they touched every signature / locked the
mirror). Verified: parity agrees to ~5e-15 over 400 samples.

## Tier 1 — New filters (high value, each keeps the identity)

- [x] **Alpha-beta (constant-velocity) tracker** — the honest "Kalman with Q"; carries a velocity
      state so it tracks ramps. 5 floats, ~6 flops, no matrix algebra. Verified: tracks a ramp with
      ~0 steady-state lag (`test_alphabeta_zero_ramp_lag`). `ab_init_tracking(r,...)` derives stable
      critically-damped gains from one parameter.
- [x] **DC blocker / 1st-order high-pass** — the library had no high-pass at all. Strips sensor
      bias / slow drift (gravity from an accelerometer, ECG/EMG baseline wander). 3 floats, seeds on
      first sample. Verified: drives a constant offset to 0.
- [x] **Complementary filter** — cheap gyro+accel fusion. The one documented exception to the
      single-input `update` convention (inherently two inputs).
- [x] **Biquad 2nd-order IIR (DF2T)** + coefficient-design helpers — sharp low/high/band-pass and
      **50/60 Hz notch**. Runtime is 7 floats and libm-free; the RBJ `sin/cos` design helpers live in
      the **optional** `src/k_filter_design.c` (needs libm) so the freestanding core stays clean.
      Verified: low-pass has unity DC gain; notch attenuates a 50 Hz tone; C↔Python design parity.
- [x] **`kf_float_t` configurable scalar type** — one typedef compiles the whole library as
      `float` (default) or `double`. *(Delivered in Tier 0.)*
- [ ] **Compile-time feature toggles** (`KF_ENABLE_*`) + `_Static_assert` config validation +
      grep-able `KF_NO_HEAP` marker. *(`KF_NO_HEAP` marker already added in Tier 0.)*
- [x] **C-vs-Python parity harness** — a checked-in input vector run through both sides, asserted
      equal. Keeps the teaching mirror honest. Now covers 8 filters. *(Delivered in Tier 0.)*
- [x] **Python metrics engine** — RMSE, lag-compensated RMSE, error-reduction (dB), lag
      (cross-correlation), overshoot/settling. Ranked side-by-side table in `sim/metrics.py` +
      the `sim/compare.py` workbench (`make compare`). Pure-Python, runs in CI.
- [x] **Python test-signal library** — step / ramp / impulse / square / chirp / sine + spike
      injection + CSV import, in `sim/signals.py` (pure-Python, no numpy).
- [ ] **Footprint / worst-case-timing / ISR-reentrancy documentation table.** *(next up)*

## Tier 2 — Nice to have

- [ ] **Hampel filter** — robust outlier rejection that preserves clean samples (reuses median).
- [ ] **Slew-rate limiter + deadband / hysteresis** — signal-conditioning primitives.
- [ ] **One-Euro filter** — adaptive-cutoff low-pass for jittery interactive (touch / IMU-UI) input.
- [ ] **Optional fixed-point (Q15/Q16) variants** for FPU-less parts, behind a toggle.
- [ ] **Golden-vector regression tests + coverage gate + micro-benchmark report.**
- [ ] **Empirical Bode / step-response characterization** in the sim.
- [ ] **Failure-mode diagnostic gallery** — visualizes the (now-fixed) defects as teaching examples.
- [ ] **Interactive slider tuner + metrics-derived filter-picker.**
- [ ] **Auto-generated README hero figure + metrics table.**
- [ ] **True O(w) streaming median** (linked-list / incremental sorted array) as a refinement of the
      Tier 0 deterministic median, if profiling shows the re-sort cost matters for large windows.

---

## Anti-scope (staying true to purpose)

These were considered and **deliberately excluded** to avoid betraying the identity:

- ❌ **No external runtime dependencies in the embedded core** — no CMSIS-DSP, libm, `printf`, or any
  vendor library in the default MCU build. (A CMSIS-DSP accelerated backend was proposed and dropped.)
- ❌ **No heap, ever** — all buffers stay caller-owned; keep the grep-able `KF_NO_HEAP` marker.
- ❌ **No VLAs / no data-dependent worst-case timing** — sizes are known and bounded at init.
- ❌ **libm stays out of the sample-time runtime** — anything needing `sinf/cosf/sqrtf` (biquad design
  helpers) hides behind a compile-time gate or is precomputed offline.
- ❌ **Fixed-point / double are opt-in only** — never bloat the default `float32` path.
- ❌ **Keep `update(f, x) -> float`** — the only sanctioned exception is the complementary filter.
- ❌ **Tests / CI / sim are strictly host-side** — nothing flashed to an MCU.
- ❌ **No web dashboard / server** (Plotly/Dash/Streamlit) — the matplotlib slider tuner delivers ~90%
  with zero new deps. (Proposed and dropped.)
- ❌ **No matrix EKF/UKF** — the alpha-beta tracker is the intended lightweight tracking answer.
- ❌ **Preserve the "copy two files" integration story** — CMake/Makefile are additive conveniences.

---

## Progress log

- **2026-07-15** — Roadmap authored. Starting Tier 0, correctness-first
  (Kalman-Q → streaming median → validation → warm-up → tests).
- **2026-07-15** — **Tier 0 complete.** All 5 filters hardened; runtime is now truly
  dependency-free (no `<stdlib.h>`); `kf_status_t` validation, uniform warm-up, `reset`/`peek`/
  `set_alpha`, `uint16_t` sizes, `kf_float_t`, wide MA accumulator. 38 unit checks pass under
  gcc/clang `-Werror`; CMake + ctest green; C-vs-Python parity ~5e-15 over 400 samples; example shows
  Kalman tracking. CI (tests × gcc/clang, parity, cppcheck, arm freestanding + include guardrail) added.
  `kf_float_t` and the parity harness pulled forward from Tier 1.
- **2026-07-15** — **Tier 1 filters complete.** Added 4 new filters — alpha-beta tracker (zero ramp
  lag), DC blocker / high-pass, complementary (2-input fusion), and biquad (DF2T) with an optional
  RBJ coefficient designer (`src/k_filter_design.c`, libm-isolated). Library now has **9 filters**.
  **59 unit checks** pass under `-Werror`; parity harness extended to **8 filters** (~5e-15); core
  object verified free of malloc/qsort/trig/printf.
- **2026-07-15** — **Python decision workbench (It.3).** Added `sim/signals.py` (step/ramp/impulse/
  square/chirp/sine + spike injection + CSV import) and `sim/metrics.py` (RMSE, lag-compensated RMSE,
  error-reduction dB, cross-correlation lag, overshoot, settling), driven by `sim/compare.py`
  (`make compare`) which ranks the smoothers per signal with a plain-language recommendation. All
  pure-Python (no numpy) and smoke-tested in CI. **Next (It.4): footprint/timing docs + compile-time
  feature toggles** to close Tier 1.
