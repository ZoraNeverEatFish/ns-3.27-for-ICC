#!/usr/bin/env python3
"""
plot_iccG_qd_rate.py — Dual-Y plot: Qd, Sending Rate & cwnd for the first flow.

Parses NS3 log lines:
  0x<addr> rttI ... Qd <s> ... now <s> ... CRate <Mbps> ... cwnd <pkts> ...

Plots (pure pycairo, no matplotlib required):
  Left  Y: Queueing Delay  Qd    (ms)
  Right Y: Sending Rate    CRate (Mbps)  /  cwnd (pkts)

Usage:
  python3 plot_iccG_qd_rate.py [input_file] [output_image]
"""

import sys, re, math
import cairo
from PIL import Image

# ═══════════════════════════════════════════════════════════════════════
# 1. Parse log
# ═══════════════════════════════════════════════════════════════════════
input_file = sys.argv[1] if len(sys.argv) > 1 else "Testdata/Wavelet/ICC-G/output.txt"
output_img = sys.argv[2] if len(sys.argv) > 2 else "Testdata/Wavelet/ICC-G/qd_rate.png"

data_re = re.compile(r"^0x[0-9a-fA-F]+ .+")
first_flow = None
times, qd_ms, rate_mbps, cwnd_pkts = [], [], [], []

with open(input_file, "r") as f:
    for line in f:
        if not data_re.match(line):
            continue
        tokens = line.split()
        flow_id = tokens[0]
        if first_flow is None:
            first_flow = flow_id
        if flow_id != first_flow:
            continue

        qd_v = rate_v = now_v = cwnd_v = None
        i = 1
        while i + 1 < len(tokens):
            k, v = tokens[i], tokens[i + 1]
            if k == "Qd":       qd_v   = float(v)
            elif k == "CRate":  rate_v = float(v)
            elif k == "now":    now_v  = float(v)
            elif k == "cwnd":   cwnd_v = float(v)
            i += 2

        if now_v is not None and qd_v is not None and rate_v is not None and cwnd_v is not None:
            times.append(now_v)
            qd_ms.append(qd_v * 1000.0)
            rate_mbps.append(rate_v)
            cwnd_pkts.append(cwnd_v)

print(f"Flow: {first_flow}  |  data points: {len(times)}")
print(f"Qd range:    [{min(qd_ms):.3f}, {max(qd_ms):.3f}] ms")
print(f"CRate range: [{min(rate_mbps):.3f}, {max(rate_mbps):.3f}] Mbps")
print(f"cwnd range:  [{min(cwnd_pkts):.3f}, {max(cwnd_pkts):.3f}] pkts")

# ═══════════════════════════════════════════════════════════════════════
# CSV export (before plotting)
# ═══════════════════════════════════════════════════════════════════════
csv_file = output_img.rsplit(".", 1)[0] + ".csv"
with open(csv_file, "w") as cf:
    cf.write("Time_s,Qd_ms,CRate_Mbps,cwnd_pkts\n")
    for i in range(len(times)):
        cf.write(f"{times[i]:.6f},{qd_ms[i]:.6f},{rate_mbps[i]:.6f},{cwnd_pkts[i]:.3f}\n")
print(f"CSV  → {csv_file}")

if len(times) < 2:
    print("ERROR: need at least 2 data points")
    sys.exit(1)

# ═══════════════════════════════════════════════════════════════════════
# 2. Canvas geometry
# ═══════════════════════════════════════════════════════════════════════
W, H = 1600, 700
LM, RM, TM, BM = 120, 120, 60, 90          # margins
PW = W - LM - RM                            # plot width
PH = H - TM - BM                            # plot height

# ═══════════════════════════════════════════════════════════════════════
# 3. Axis ranges
# ═══════════════════════════════════════════════════════════════════════
t_min, t_max = times[0], times[-1]
if t_max <= t_min:
    t_max = t_min + 1.0

qd_min, qd_max = min(qd_ms), max(qd_ms)
qd_margin = max((qd_max - qd_min) * 0.08, 0.01)
qd_min -= qd_margin
qd_max += qd_margin

rt_min, rt_max = min(rate_mbps), max(rate_mbps)
rt_margin = max((rt_max - rt_min) * 0.08, 0.01)
rt_min -= rt_margin
rt_max += rt_margin

def tx(x):
    return LM + (x - t_min) / (t_max - t_min) * PW

def ty_qd(y):
    return TM + PH - (y - qd_min) / (qd_max - qd_min) * PH

def ty_rt(y):
    return TM + PH - (y - rt_min) / (rt_max - rt_min) * PH

# ═══════════════════════════════════════════════════════════════════════
# 4. Draw
# ═══════════════════════════════════════════════════════════════════════
surf = cairo.ImageSurface(cairo.FORMAT_ARGB32, W, H)
ctx = cairo.Context(surf)

# background
ctx.set_source_rgb(1, 1, 1)
ctx.paint()

# ── grid & axes ───────────────────────────────────────────────────
ctx.set_line_width(0.6)
ctx.set_source_rgba(0.85, 0.85, 0.85, 0.7)

# 5 vertical grid lines
for i in range(6):
    frac = i / 5.0
    xi = LM + frac * PW
    ctx.move_to(xi, TM)
    ctx.line_to(xi, TM + PH)
ctx.stroke()

# 5 horizontal grid lines
for i in range(6):
    frac = i / 5.0
    yi = TM + frac * PH
    ctx.move_to(LM, yi)
    ctx.line_to(LM + PW, yi)
ctx.stroke()

# ── plot frame ────────────────────────────────────────────────────
ctx.set_line_width(1.2)
ctx.set_source_rgb(0.2, 0.2, 0.2)
ctx.rectangle(LM, TM, PW, PH)
ctx.stroke()

# ── render Qd line (blue) ─────────────────────────────────────────
ctx.set_source_rgb(0.12, 0.47, 0.85)
ctx.set_line_width(0.7)
ctx.move_to(tx(times[0]), ty_qd(qd_ms[0]))
for i in range(1, len(times)):
    ctx.line_to(tx(times[i]), ty_qd(qd_ms[i]))
ctx.stroke()

# ── render CRate line (red) ───────────────────────────────────────
ctx.set_source_rgb(0.85, 0.33, 0.00)
ctx.set_line_width(0.7)
ctx.move_to(tx(times[0]), ty_rt(rate_mbps[0]))
for i in range(1, len(times)):
    ctx.line_to(tx(times[i]), ty_rt(rate_mbps[i]))
ctx.stroke()

# ── render cwnd line (green, right axis) ──────────────────────────
ctx.set_source_rgb(0.13, 0.55, 0.13)
ctx.set_line_width(0.9)
ctx.move_to(tx(times[0]), ty_rt(cwnd_pkts[0]))
for i in range(1, len(times)):
    ctx.line_to(tx(times[i]), ty_rt(cwnd_pkts[i]))
ctx.stroke()

# ═══════════════════════════════════════════════════════════════════════
# 5. Labels & ticks
# ═══════════════════════════════════════════════════════════════════════
ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)

def label(text, x, y, size=13, color=(0.15, 0.15, 0.15), center=False):
    ctx.set_font_size(size)
    ctx.set_source_rgb(*color)
    if center:
        xb, yb, tw, th, _, _ = ctx.text_extents(text)
        x -= tw / 2
    ctx.move_to(x, y)
    ctx.show_text(text)

# Y-axis tick labels — left (Qd)
for i in range(6):
    frac = i / 5.0
    val = qd_min + frac * (qd_max - qd_min)
    yi = ty_qd(val)
    label(f"{val:.3f}", LM - 10, yi + 4, size=11, center=True)
    # tick dash
    ctx.set_source_rgba(0.2, 0.2, 0.2, 0.5)
    ctx.set_line_width(0.6)
    ctx.move_to(LM - 5, yi)
    ctx.line_to(LM, yi)
    ctx.stroke()

# Y-axis tick labels — right (CRate)
for i in range(6):
    frac = i / 5.0
    val = rt_min + frac * (rt_max - rt_min)
    yi = ty_rt(val)
    label(f"{val:.3f}", LM + PW + 10, yi + 4, size=11, center=False)
    ctx.set_source_rgba(0.2, 0.2, 0.2, 0.5)
    ctx.set_line_width(0.6)
    ctx.move_to(LM + PW, yi)
    ctx.line_to(LM + PW + 5, yi)
    ctx.stroke()

# X-axis tick labels
num_x = min(8, int(t_max - t_min) + 1)
for i in range(num_x + 1):
    frac = i / max(num_x, 1)
    val = t_min + frac * (t_max - t_min)
    xi = tx(val)
    if xi < LM - 5 or xi > LM + PW + 5:
        continue
    label(f"{val:.1f}", xi, TM + PH + 25, size=11, center=True)

# ── Y-axis titles ─────────────────────────────────────────────────
ctx.save()
ctx.set_source_rgb(0.12, 0.47, 0.85)
ctx.set_font_size(15)
ctx.move_to(16, TM + PH / 2 + 50)
ctx.rotate(-math.pi / 2)
ctx.show_text("Queueing Delay (ms)")
ctx.restore()

ctx.save()
ctx.set_source_rgb(0.85, 0.33, 0.00)
ctx.set_font_size(15)
ctx.move_to(LM + PW + 32, TM + PH / 2 - 50)
ctx.rotate(math.pi / 2)
ctx.show_text("Sending Rate (Mbps)")
ctx.restore()

# ── X-axis title ──────────────────────────────────────────────────
ctx.set_source_rgb(0.15, 0.15, 0.15)
ctx.set_font_size(15)
label("Time (s)", LM + PW / 2, TM + PH + 55, size=15, center=True)

# ── Title ─────────────────────────────────────────────────────────
ctx.set_font_size(18)
ctx.set_source_rgb(0.05, 0.05, 0.05)
title = f"ICC-G — Flow {first_flow}"
label(title, W / 2, 34, size=18, center=True)

# ── Legend ────────────────────────────────────────────────────────
lx, ly = LM + 16, TM + 20
ctx.set_source_rgb(0.12, 0.47, 0.85)
ctx.set_line_width(2.5)
ctx.move_to(lx, ly)
ctx.line_to(lx + 40, ly)
ctx.stroke()
label("Queueing Delay (Qd)", lx + 48, ly + 6, size=13)

ctx.set_source_rgb(0.85, 0.33, 0.00)
ctx.set_line_width(2.5)
ctx.move_to(lx + 200, ly)
ctx.line_to(lx + 240, ly)
ctx.stroke()
label("Sending Rate (CRate)", lx + 248, ly + 6, size=13)

ctx.set_source_rgb(0.13, 0.55, 0.13)
ctx.set_line_width(2.5)
ctx.move_to(lx + 420, ly)
ctx.line_to(lx + 460, ly)
ctx.stroke()
label("cwnd (pkts)", lx + 468, ly + 6, size=13)

# ═══════════════════════════════════════════════════════════════════════
# 6. Save
# ═══════════════════════════════════════════════════════════════════════
surf.write_to_png(output_img)
print(f"Saved → {output_img}")
