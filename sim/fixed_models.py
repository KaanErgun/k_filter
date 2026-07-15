"""Bit-exact integer reference models for the HDL (Verilog/VHDL) filters.

These are the golden reference: the Verilog and VHDL modules must reproduce these
outputs EXACTLY, sample-for-sample, bit-for-bit. Everything is integer arithmetic
with arithmetic right shift (Python `>>` floors negatives, matching signed `>>>`
in Verilog and `shift_right` in VHDL). The EMA also matches the C `ema_fixed`
(src/k_filter_fixed.c), so an embedded dev gets identical numbers on an MCU (C),
an FPGA/ASIC (HDL), and in Python — making it easy to validate a design.

Pure-Python (stdlib only).
"""


def _sat(v, width):
    hi = (1 << (width - 1)) - 1
    lo = -(1 << (width - 1))
    if v > hi:
        return hi
    if v < lo:
        return lo
    return v


def ema_q15(samples, alpha_q15, width=16):
    """1st-order low-pass / EMA with a Q15 coefficient. Seeds on first sample.
    y += (alpha * (x - y)) >> 15, saturated to `width` bits."""
    y = 0
    started = False
    out = []
    for x in samples:
        if not started:
            y = x
            started = True
        else:
            diff = x - y
            inc = (alpha_q15 * diff) >> 15      # arithmetic shift (floor)
            y = _sat(y + inc, width)
        out.append(y)
    return out


def moving_avg_hw(samples, log2n, width=16):
    """Power-of-2 boxcar moving average, hardware form: fills from zero (no
    count-normalized warm-up) and divides by N with an arithmetic shift.
    y = sum(last 2**log2n samples) >> log2n."""
    n = 1 << log2n
    buf = [0] * n
    idx = 0
    s = 0
    out = []
    for x in samples:
        s -= buf[idx]
        buf[idx] = x
        s += x
        idx = (idx + 1) % n
        out.append(_sat(s >> log2n, width))
    return out


def dc_blocker_q15(samples, r_q15, width=16):
    """1st-order high-pass / DC blocker: y = x - x_prev + (r * y_prev) >> 15,
    r in Q15 (e.g. 32604 ~= 0.995). Seeds on first sample (first output 0)."""
    x_prev = 0
    y_prev = 0
    started = False
    out = []
    for x in samples:
        if not started:
            x_prev = x
            y_prev = 0
            started = True
            out.append(0)
            continue
        y = _sat(x - x_prev + ((r_q15 * y_prev) >> 15), width)
        x_prev = x
        y_prev = y
        out.append(y)
    return out


# Shared config so the HDL testbenches and Python use identical coefficients.
WIDTH = 16
EMA_ALPHA_Q15 = 3277      # ~0.1  (round(0.1 * 32768))
MAVG_LOG2N = 3            # window = 8
DC_R_Q15 = 32604          # ~0.995

MODELS = {
    "ema_q15":       lambda xs: ema_q15(xs, EMA_ALPHA_Q15, WIDTH),
    "moving_avg":    lambda xs: moving_avg_hw(xs, MAVG_LOG2N, WIDTH),
    "dc_blocker_q15": lambda xs: dc_blocker_q15(xs, DC_R_Q15, WIDTH),
}
