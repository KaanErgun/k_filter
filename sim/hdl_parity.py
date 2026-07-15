#!/usr/bin/env python3
"""HDL bit-exact parity harness.

Feeds one deterministic int16 stimulus vector through, for each filter:
  - the Python golden model (sim/fixed_models.py),
  - the Verilog module   (hdl/verilog/*.v)  via Icarus Verilog,
  - the VHDL module      (hdl/vhdl/*.vhd)    via GHDL,
  - and, for the EMA, the C fixed-point filter (src/k_filter_fixed.c),
then asserts every engine produces EXACTLY the same output, sample-for-sample.
This lets an embedded dev validate that their MCU (C), FPGA/ASIC (HDL), and
Python model all agree to the bit — so a design decision carries across targets.

Missing tools (iverilog / ghdl / cc) are skipped with a note, not failed.
Pure-Python stdlib.
"""
import math
import os
import random
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "sim"))
import fixed_models as fm  # noqa: E402

FILTERS = [
    dict(name="ema_q15", vdef="F_EMA", vsrc="ema_q15.v",
         vhd="ema_q15.vhd", tb="tb_ema.vhd", ent="tb_ema", c=True),
    dict(name="moving_avg", vdef="F_MAVG", vsrc="moving_avg.v",
         vhd="moving_avg.vhd", tb="tb_mavg.vhd", ent="tb_mavg", c=False),
    dict(name="dc_blocker_q15", vdef="F_DC", vsrc="dc_blocker_q15.v",
         vhd="dc_blocker_q15.vhd", tb="tb_dc.vhd", ent="tb_dc", c=False),
]


def make_stimulus(n=200):
    rng = random.Random(2024)
    xs = []
    for i in range(n):
        base = int(9000 * math.sin(i * 0.13) + 3000 * math.sin(i * 0.017))
        noise = rng.randint(-400, 400)
        spike = (14000 if rng.random() > 0.5 else -14000) if (i % 29 == 0 and i > 0) else 0
        xs.append(max(-32768, min(32767, base + noise + spike)))
    return xs


def _ints(text):
    out = []
    for line in text.splitlines():
        s = line.strip()
        if s and (s.lstrip("-").isdigit()):
            out.append(int(s))
    return out


def run_verilog(work, f):
    vvp = os.path.join(work, f["name"] + ".vvp")
    subprocess.run(
        ["iverilog", "-g2012", "-D" + f["vdef"], "-o", vvp,
         os.path.join(ROOT, "hdl/verilog/tb.v"),
         os.path.join(ROOT, "hdl/verilog", f["vsrc"])],
        cwd=work, check=True, capture_output=True, text=True)
    r = subprocess.run(["vvp", vvp], cwd=work, capture_output=True, text=True)
    return _ints(r.stdout)


def run_vhdl(work, f):
    subprocess.run(["ghdl", "-a", "--std=08",
                    os.path.join(ROOT, "hdl/vhdl", f["vhd"]),
                    os.path.join(ROOT, "hdl/vhdl", f["tb"])],
                   cwd=work, check=True, capture_output=True, text=True)
    # Elaborate: try plain, then with an x86_64 link override (Intel ghdl on Apple Silicon).
    for extra in ([], ["-Wl,-arch", "-Wl,x86_64"]):
        subprocess.run(["ghdl", "-e", "--std=08"] + extra + [f["ent"]],
                       cwd=work, capture_output=True, text=True)
        r = subprocess.run(["ghdl", "-r", "--std=08", f["ent"]],
                           cwd=work, capture_output=True, text=True)
        got = _ints(r.stdout)
        if got:
            return got
    return []


def run_c(work, xs):
    exe = os.path.join(work, "cdump")
    subprocess.run(
        ["cc", "-std=c99", "-I" + os.path.join(ROOT, "include"),
         os.path.join(ROOT, "test/fixed_dump.c"),
         os.path.join(ROOT, "src/k_filter_fixed.c"), "-o", exe],
        check=True, capture_output=True, text=True)
    r = subprocess.run([exe], input="\n".join(map(str, xs)) + "\n",
                       capture_output=True, text=True)
    return _ints(r.stdout)


def main():
    have = {t: shutil.which(t) is not None for t in ("iverilog", "vvp", "ghdl", "cc")}
    have_v = have["iverilog"] and have["vvp"]
    print("tools:", ", ".join(f"{t}={'yes' if v else 'NO'}" for t, v in have.items()))

    xs = make_stimulus()
    work = tempfile.mkdtemp(prefix="kf_hdl_")
    with open(os.path.join(work, "stimulus.txt"), "w") as fh:
        fh.write("\n".join(map(str, xs)) + "\n")

    print(f"\nstimulus: {len(xs)} int16 samples\n")
    hdr = f"{'filter':16}{'samples':>8}  {'Verilog':>9}{'VHDL':>7}{'C':>5}   verdict"
    print(hdr)
    print("-" * len(hdr))

    fails = 0
    for f in FILTERS:
        golden = fm.MODELS[f["name"]](xs)

        def check(engine_ok, got):
            if not engine_ok:
                return "skip"
            return "ok" if got == golden else "FAIL"

        v = run_verilog(work, f) if have_v else None
        vh = run_vhdl(work, f) if have["ghdl"] else None
        c = run_c(work, xs) if (f["c"] and have["cc"]) else None

        sv = check(have_v, v)
        svh = check(have["ghdl"], vh)
        sc = "—" if not f["c"] else check(have["cc"], c)

        engines = [s for s in (sv, svh, sc) if s not in ("skip", "—")]
        verdict = "BIT-EXACT ✓" if engines and all(s == "ok" for s in engines) else "MISMATCH ✗"
        if "FAIL" in (sv, svh, sc):
            fails += 1

        def cell(s):
            return {"ok": "✓", "FAIL": "✗", "skip": "skip", "—": "—"}[s]

        print(f"{f['name']:16}{len(golden):>8}  {cell(sv):>9}{cell(svh):>7}{cell(sc):>5}   {verdict}")

    shutil.rmtree(work, ignore_errors=True)
    if fails:
        print(f"\nFAILED: {fails} filter(s) mismatched")
        return 1
    print("\nPASSED: Python == Verilog == VHDL"
          + (" == C" if have["cc"] else "") + " (bit-exact)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
