# k_filter — HDL (Verilog & VHDL)

Synthesizable fixed-point filters for FPGA/ASIC, **bit-exact** with the C
(`src/k_filter_fixed.c`) and the Python golden models (`sim/fixed_models.py`).
Prototype in Python, then drop the identical behavior onto an MCU (C) or fabric (HDL).

## Modules

| Module | Kind | Coefficient(s) |
|--------|------|----------------|
| `ema_q15` | 1st-order low-pass / EMA | `ALPHA` (Q15, 0..32767) |
| `moving_avg` | power-of-2 boxcar average | `LOG2N` (window = 2^LOG2N) |
| `dc_blocker_q15` | 1st-order high-pass / DC blocker | `R` (Q15 pole, ~0.995) |
| `fir5` | 5-tap FIR low-pass | fixed Q15 taps (sum = 1.0) |
| `biquad_df1` | 2nd-order IIR, Direct Form I | fixed Q12 RBJ low-pass |
| `median5` | median of a 5-sample window | — (nonlinear rank) |
| `slew_limiter` | slew-rate limiter | `MAX_STEP` |
| `deadband` | deadband / hysteresis hold | `THRESHOLD` |

Each exists as Verilog (`verilog/*.v`) and VHDL (`vhdl/*.vhd`). FIR/biquad coefficients are fixed
constants shared verbatim with the Python model (`sim/fixed_models.py`); change them there and in the
HDL together. `median5` uses a small bubble sort in an HDL function — correct by construction and
checked bit-for-bit by `make hdl`.

## Interface (identical for every module)

```
clk        in   — rising-edge clock
rst        in   — synchronous, active-high reset
in_valid   in   — assert for one cycle per input sample
x          in   — signed [WIDTH-1:0] input sample
y          out  — signed [WIDTH-1:0] filtered output
out_valid  out  — high the cycle `y` is valid
```

Generics/parameters: `WIDTH` (data width, default 16) and the coefficient(s) above.
Single-cycle latency: with `in_valid` high every cycle, one output is produced per cycle.

## Fixed-point contract

All math is integer. The Q15 requantization is an **arithmetic right shift by 15** (floor), matching
signed `>>>` in Verilog, `shift_right` in VHDL, `>>` in Python, and `>>` in C — which is why all four
agree to the bit. Outputs are saturated to the signed `WIDTH` range.

## Simulate / verify

From the repo root:

```bash
make hdl        # Python vs Verilog (iverilog) vs VHDL (ghdl) vs C — asserts bit-exact
```

Or by hand (Verilog):

```bash
iverilog -g2012 -DF_EMA -o ema.vvp hdl/verilog/tb.v hdl/verilog/ema_q15.v
vvp ema.vvp          # reads ./stimulus.txt (one signed decimal per line)
```

VHDL:

```bash
ghdl -a --std=08 hdl/vhdl/ema_q15.vhd hdl/vhdl/tb_ema.vhd
ghdl -e --std=08 tb_ema && ghdl -r --std=08 tb_ema   # reads ./stimulus.txt
```

> Tools: [Icarus Verilog](http://iverilog.icarus.com/) and [GHDL](https://ghdl.github.io/ghdl/).
> On Apple-Silicon macs with an Intel GHDL build, elaboration needs
> `-Wl,-arch -Wl,x86_64` (the `make hdl` harness adds it automatically as a fallback).
