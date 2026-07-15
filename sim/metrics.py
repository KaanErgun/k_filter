"""Quantitative filter-comparison metrics for the k_filter simulation.

Pure-Python (stdlib only). Turns the sim from a picture into a decision tool:
how much noise each filter removes, at what lag, and how it responds to a step.
"""
import math


def _mean(xs):
    return sum(xs) / len(xs)


def _var(xs):
    m = _mean(xs)
    return sum((x - m) ** 2 for x in xs) / len(xs)


def rmse(out, clean):
    """Root-mean-square error of the filtered output vs the ground truth."""
    n = min(len(out), len(clean))
    return math.sqrt(sum((out[i] - clean[i]) ** 2 for i in range(n)) / n)


def error_reduction_db(noisy, out, clean):
    """10*log10( var(noisy-clean) / var(out-clean) ). Positive = the filter
    reduced total error. NOTE: the denominator also captures filter-induced lag
    and distortion, so this is 'error reduction', not pure SNR gain — read it
    together with the lag column and the lag-compensated RMSE."""
    n = min(len(noisy), len(out), len(clean))
    ein = _var([noisy[i] - clean[i] for i in range(n)])
    eout = _var([out[i] - clean[i] for i in range(n)])
    if eout <= 0.0:
        return float("inf")
    return 10.0 * math.log10(ein / eout)


def lag(out, clean, max_lag=40):
    """Delay (in samples) of `out` relative to `clean`, via the cross-correlation
    peak over non-negative shifts. Mean-removed so a DC offset doesn't bias it."""
    n = min(len(out), len(clean))
    cm, om = _mean(clean[:n]), _mean(out[:n])
    c = [clean[i] - cm for i in range(n)]
    o = [out[i] - om for i in range(n)]
    best_d, best_corr = 0, -float("inf")
    upper = min(max_lag, n - 1)
    for d in range(0, upper + 1):
        s = 0.0
        for i in range(0, n - d):
            s += c[i] * o[i + d]
        if s > best_corr:
            best_corr, best_d = s, d
    return best_d


def lag_compensated_rmse(out, clean, d):
    """RMSE after shifting `out` earlier by its measured lag, isolating the
    smoothing quality from the delay."""
    n = min(len(out), len(clean))
    if d >= n:
        return float("nan")
    m = n - d
    return math.sqrt(sum((out[i + d] - clean[i]) ** 2 for i in range(m)) / m)


def overshoot_pct(out, final):
    """Peak overshoot beyond the final level, as a percentage (step response)."""
    if final == 0.0:
        return 0.0
    return max(0.0, (max(out) - final) / abs(final) * 100.0)


def settling_time(out, final, tol_frac=0.05):
    """First sample index after which the output stays within tol_frac of `final`.
    Returns -1 if it never settles."""
    band = tol_frac * (abs(final) if final != 0.0 else 1.0)
    n = len(out)
    for k in range(n):
        if all(abs(out[j] - final) <= band for j in range(k, n)):
            return k
    return -1


def evaluate(out, clean, noisy, is_step=False, final=None):
    """Bundle the metrics for one filter's output into a dict."""
    d = lag(out, clean)
    m = {
        "rmse": rmse(out, clean),
        "rmse_lc": lag_compensated_rmse(out, clean, d),
        "err_db": error_reduction_db(noisy, out, clean),
        "lag": d,
    }
    if is_step and final is not None:
        m["over"] = overshoot_pct(out, final)
        m["settle"] = settling_time(out, final)
    else:
        m["over"] = None
        m["settle"] = None
    return m


def format_table(rows, is_step=False):
    """rows: list of (name, metrics-dict). Returns an aligned ASCII table string.
    Rows are ranked best-first by lag-compensated RMSE (smoothing quality)."""
    rows = sorted(rows, key=lambda r: r[1]["rmse_lc"])
    header = ["Filter", "RMSE", "RMSE(lag-comp)", "ErrRed(dB)", "Lag"]
    if is_step:
        header += ["Over%", "Settle"]
    widths = [14, 8, 15, 11, 5, 7, 7]

    def cell(s, w):
        return str(s).ljust(w)

    def num(x, nd=3):
        if x is None:
            return "—"
        if x == float("inf"):
            return "inf"
        if isinstance(x, float) and math.isnan(x):
            return "nan"
        return f"{x:.{nd}f}"

    lines = []
    lines.append("".join(cell(h, widths[i]) for i, h in enumerate(header)))
    lines.append("".join(cell("-" * (len(h)), widths[i]) for i, h in enumerate(header)))
    for name, m in rows:
        cols = [name, num(m["rmse"]), num(m["rmse_lc"]), num(m["err_db"], 2), str(m["lag"])]
        if is_step:
            settle = m["settle"]
            cols += [num(m["over"], 1) if m["over"] is not None else "—",
                     "—" if settle in (None, -1) else str(settle)]
        lines.append("".join(cell(c, widths[i]) for i, c in enumerate(cols)))
    return "\n".join(lines)
