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


def slew_limiter(samples, max_step, width=16):
    """Bounds |output change| per sample. Seeds on first sample."""
    y = 0
    started = False
    out = []
    for x in samples:
        if not started:
            y = x
            started = True
        else:
            d = x - y
            if d > max_step:
                d = max_step
            elif d < -max_step:
                d = -max_step
            y = _sat(y + d, width)
        out.append(y)
    return out


def deadband(samples, threshold, width=16):
    """Holds the last output until the input moves beyond `threshold`."""
    y = 0
    started = False
    out = []
    for x in samples:
        if not started:
            y = x
            started = True
        elif abs(x - y) > threshold:
            y = x
        out.append(y)
    return out


def fir5(samples, coeffs, qshift, width=16):
    """5-tap FIR. y = (sum coeff[i]*tap[i]) >> qshift, taps fill from zero.
    coeffs[0] pairs with the newest sample."""
    n = len(coeffs)
    taps = [0] * n
    out = []
    for x in samples:
        taps = [x] + taps[:-1]
        acc = sum(c * t for c, t in zip(coeffs, taps))
        out.append(_sat(acc >> qshift, width))
    return out


def biquad_df1(samples, b, a, qshift, width=16):
    """2nd-order IIR, Direct Form I, integer coefficients (a0 normalized to 1).
    acc = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;  y = sat(acc >> qshift)."""
    x1 = x2 = y1 = y2 = 0
    out = []
    for x in samples:
        acc = b[0] * x + b[1] * x1 + b[2] * x2 - a[0] * y1 - a[1] * y2
        y = _sat(acc >> qshift, width)
        x2, x1 = x1, x
        y2, y1 = y1, y
        out.append(y)
    return out


def median5(samples, width=16):
    """Median of a fixed 5-sample window (fills from zero)."""
    win = [0] * 5
    out = []
    for x in samples:
        win = [x] + win[:-1]
        out.append(_sat(sorted(win)[2], width))
    return out


# Shared config so the HDL testbenches and Python use identical coefficients.
WIDTH = 16
EMA_ALPHA_Q15 = 3277      # ~0.1  (round(0.1 * 32768))
MAVG_LOG2N = 3            # window = 8
DC_R_Q15 = 32604          # ~0.995
SLEW_MAX_STEP = 2000
DEADBAND_THRESHOLD = 500
FIR5_COEFFS = [2048, 8192, 12288, 8192, 2048]   # Q15, sum = 32768 (unity DC)
FIR5_QSHIFT = 15
# RBJ low-pass, fc/fs=0.1, Q=0.707, quantized to Q12 (see the generating comment):
BIQUAD_B = [276, 553, 276]
BIQUAD_A = [-4682, 1691]
BIQUAD_QSHIFT = 12

MODELS = {
    "ema_q15":        lambda xs: ema_q15(xs, EMA_ALPHA_Q15, WIDTH),
    "moving_avg":     lambda xs: moving_avg_hw(xs, MAVG_LOG2N, WIDTH),
    "dc_blocker_q15": lambda xs: dc_blocker_q15(xs, DC_R_Q15, WIDTH),
    "slew_limiter":   lambda xs: slew_limiter(xs, SLEW_MAX_STEP, WIDTH),
    "deadband":       lambda xs: deadband(xs, DEADBAND_THRESHOLD, WIDTH),
    "fir5":           lambda xs: fir5(xs, FIR5_COEFFS, FIR5_QSHIFT, WIDTH),
    "biquad_df1":     lambda xs: biquad_df1(xs, BIQUAD_B, BIQUAD_A, BIQUAD_QSHIFT, WIDTH),
    "median5":        lambda xs: median5(xs, WIDTH),
}
