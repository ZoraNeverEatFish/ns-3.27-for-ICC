#!/usr/bin/env python3
"""
process_CN.py — Compute ideal & measured C/N from Testdata/CN/ICC-G/*/output.txt

Output CSV columns:
  flows, ideal_C_N_Mbps, measured_C_N_Mbps

  ideal_C_N    = 100 Mbps / flows              (fair-share)
  measured_C_N = log(10 ms / avg_Qd_ms) * 30   (from last 2s of data)

Usage:
  python3 process_CN.py [data_dir] [output_csv]
"""

import sys, os, re, math

# ═══════════════════════════════════════════════════════════════════════
data_dir  = sys.argv[1] if len(sys.argv) > 1 else "Testdata/CN/ICC-G"
out_csv   = sys.argv[2] if len(sys.argv) > 2 else "Testdata/CN/CN_results.csv"
BW        = 100.0          # bottleneck bandwidth (Mbps)
Bd_ms     = 10.0           # target Bdelay (ms)
Rc        = 30.0           # Rc parameter

data_re   = re.compile(r"^0x[0-9a-fA-F]+ .+")

# ═══════════════════════════════════════════════════════════════════════
results = []
folders = sorted([d for d in os.listdir(data_dir)
                  if os.path.isdir(os.path.join(data_dir, d)) and d.isdigit()],
                 key=int)

for folder in folders:
    fpath = os.path.join(data_dir, folder, "output.txt")
    if not os.path.isfile(fpath):
        print(f"SKIP {folder}: no output.txt")
        continue

    flows = int(folder)
    ideal_cn = BW / flows

    # ── parse last-2s Qd values ───────────────────────────────────
    qd_vals = []       # (t, qd)  — qd in seconds
    with open(fpath, "r") as f:
        for line in f:
            if not data_re.match(line):
                continue
            tokens = line.split()
            qd_v = now_v = None
            i = 1
            while i + 1 < len(tokens):
                k, v = tokens[i], tokens[i + 1]
                if k == "Qd":    qd_v  = float(v)
                elif k == "now": now_v = float(v)
                i += 2
            if now_v is not None and qd_v is not None:
                qd_vals.append((now_v, qd_v))

    if not qd_vals:
        print(f"SKIP {folder}: no data")
        continue

    t_max = qd_vals[-1][0]
    t_cut = t_max - 2.0

    # average Qd in the last 2 seconds
    qd_sum, qd_n = 0.0, 0
    for t, q in qd_vals:
        if t >= t_cut:
            qd_sum += q
            qd_n   += 1

    if qd_n == 0:
        print(f"SKIP {folder}: no data in last 2s")
        continue

    avg_qd_s  = qd_sum / qd_n
    avg_qd_ms = avg_qd_s * 1000.0

    # measured C/N = log(10ms / avg_Qd_ms) * 30 Mbps
    if avg_qd_ms > 0:
        measured_cn = math.log(Bd_ms / avg_qd_ms) * Rc
    else:
        measured_cn = float('inf')

    results.append((flows, ideal_cn, avg_qd_ms, measured_cn))
    print(f"flows={flows:2d}  t_max={t_max:.1f}s  "
          f"last2s_points={qd_n:5d}  avg_Qd={avg_qd_ms:.4f} ms  "
          f"ideal_C/N={ideal_cn:.4f} Mbps  measured_C/N={measured_cn:.4f} Mbps")

# ═══════════════════════════════════════════════════════════════════════
# Write CSV
# ═══════════════════════════════════════════════════════════════════════
with open(out_csv, "w") as f:
    f.write("flows,ideal_C_N_Mbps,avg_Qd_ms,measured_C_N_Mbps\n")
    for flows, ideal_cn, avg_qd_ms, measured_cn in results:
        f.write(f"{flows},{ideal_cn:.6f},{avg_qd_ms:.6f},{measured_cn:.6f}\n")

print(f"\nSaved → {out_csv}")
