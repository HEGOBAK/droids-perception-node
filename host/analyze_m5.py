"""M5 log analysis (B3 timing, B6 paired trials).

Usage:  python3 host/analyze_m5.py results/M5/b6-*.txt

Per file: dt stats, RMS / max |e|, % saturated, settling time after each turn,
and a plot of e, theta and yaw → figures/M5/<name>.png.
At the end: one table, mean per group (stab-on / stab-off / others).
"""
import sys, os, math, statistics
import matplotlib
matplotlib.use("Agg")                   # save to file, no window
import matplotlib.pyplot as plt

PERIOD_MS = 20.0        # expected control period
LATE_MS = 25.0          # a tick longer than this counts as late
SETTLE_E = 0.2          # "settled" = |e| stays below this
TURN_DEG = 1.5          # yaw change over TURN_ROWS that means "a turn started"
TURN_ROWS = 25          # 25 rows = 0.5 s (catches slow turns, ~3 °/s and up)
TURN_GAP_S = 4.0        # ignore new turns this soon after the last one (a slow turn takes ~3 s; holds are 5 s)
E_TO_DEG = 26.3         # e = 1 ≈ 26.3° (A6: half field of view)
START_SKIP_S = 1.0      # first second = servo slewing from 0° to the target (rate limit) → not counted


def read_log(path):
    """Return lists of columns from the LOG rows.
    LOG,time_s,dt_ms,fresh,age_ms,cx,e,I,theta_track,yaw,theta_stab,theta,clamped,imu_valid"""
    t, dt, e, theta, yaw, clamped, ages = [], [], [], [], [], [], []
    for line in open(path):
        line = line.strip()
        if line.startswith("LOG: "):
            line = "LOG," + line[5:]
        if not line.startswith("LOG,"):
            continue                                    # header, TIMING, boot text
        f = line.split(",")
        t.append(float(f[1]))
        dt.append(float(f[2]))
        theta.append(float(f[11]))
        yaw.append(float(f[9]))
        clamped.append(f[12] == "1")
        if f[3] == "1":                                 # e and age are only real when fresh
            e.append(float(f[6]))
            ages.append(float(f[4]))
        else:
            e.append(float("nan"))                      # keeps e[i] lined up with t[i]
    return t, dt, e, theta, yaw, clamped, ages


def find_turns(t, yaw):
    """Index of the row where each turn starts (yaw moves > TURN_DEG in 0.5 s)."""
    turns = []
    last_turn_t = -1e9
    for i in range(TURN_ROWS, len(yaw)):
        if abs(yaw[i] - yaw[i - TURN_ROWS]) > TURN_DEG and t[i] - last_turn_t > TURN_GAP_S:
            turns.append(i - TURN_ROWS)                 # the turn began at the start of that 0.5 s
            last_turn_t = t[i]
    return turns


def settling_times(t, e, turns):
    """For each turn: seconds until |e| stays below SETTLE_E, up to the next turn.
    Scan backwards from the end of the window to the last row with |e| >= SETTLE_E.
    0.0 = never left the band. None = still outside at the end of the window."""
    result = []
    for k, start in enumerate(turns):
        end = turns[k + 1] if k + 1 < len(turns) else len(e)
        last_bad = None
        for i in range(end - 1, start - 1, -1):         # backwards
            if not math.isnan(e[i]) and abs(e[i]) >= SETTLE_E:
                last_bad = i
                break
        if last_bad is None:
            result.append(0.0)
            continue
        settled = None                                  # first fresh row after the last bad one
        for i in range(last_bad + 1, end):
            if not math.isnan(e[i]):
                settled = t[i] - t[start]
                break
        result.append(settled)
    return result


def plot(path, t, e, theta, yaw, turns):
    name = os.path.splitext(os.path.basename(path))[0]  # results/M5/b6-stab-on-1.txt → b6-stab-on-1
    out = os.path.join("figures", "M5", name + ".png")
    e_deg = [x * E_TO_DEG for x in e]                   # NaN stays NaN → gap in the line

    fig, ax = plt.subplots(3, 1, figsize=(9, 7), sharex=True)
    ax[0].plot(t, e_deg, color="tab:blue")
    ax[0].axhline(+SETTLE_E * E_TO_DEG, color="grey", linestyle=":")
    ax[0].axhline(-SETTLE_E * E_TO_DEG, color="grey", linestyle=":")
    ax[0].set_ylabel("e (° ≈ e × 26.3)")
    ax[1].plot(t, theta, color="tab:orange")
    ax[1].set_ylabel("theta (°)")
    ax[2].plot(t, yaw, color="tab:green")
    ax[2].set_ylabel("yaw (°)")
    ax[2].set_xlabel("time (s)")
    for a in ax:
        a.grid(True, alpha=0.3)
        for i in turns:
            a.axvline(t[i], color="red", alpha=0.3)     # where each turn starts
    fig.suptitle(name)
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)
    return out


def analyze(path):
    t, dt, e, theta, yaw, clamped, ages = read_log(path)
    if len(t) < 2:
        print(f"{path}: need at least two LOG rows")
        return None

    dts = dt[1:]                                        # first dt is measured from start-up
    deviations = sorted(abs(x - PERIOD_MS) for x in dts)
    p95 = deviations[math.ceil(0.95 * len(deviations)) - 1]   # nearest-rank (M1)
    late = sum(1 for x in dts if x > LATE_MS)

    # e and saturation skip the start-up second (servo slewing to the target, rate-limited)
    keep = [i for i in range(len(t)) if t[i] - t[0] >= START_SKIP_S]
    real_e = [e[i] for i in keep if not math.isnan(e[i])]
    rms_e = math.sqrt(sum(x * x for x in real_e) / len(real_e)) if real_e else float("nan")
    max_e = max(abs(x) for x in real_e) if real_e else float("nan")
    sat = 100.0 * sum(clamped[i] for i in keep) / len(keep)

    turns = find_turns(t, yaw)
    settle = settling_times(t, e, turns)
    figure = plot(path, t, e, theta, yaw, turns)

    print(f"== {path}")
    print(f"  rows {len(t)} ({t[-1] - t[0]:.0f} s), fresh {len(real_e)}")
    print(f"  dt mean {statistics.mean(dts):.3f} ms, p95 |dt-20| {p95:.3f} ms, max {max(dts):.2f} ms, late {late}")
    if ages:
        print(f"  max age (fresh) {max(ages):.0f} ms")
    print(f"  (from {START_SKIP_S:.0f} s on) RMS e {rms_e:.3f} ({rms_e * E_TO_DEG:.1f}°), max |e| {max_e:.3f} ({max_e * E_TO_DEG:.1f}°), saturated {sat:.1f}%")
    if turns:
        shown = ", ".join("not settled" if s is None else f"{s:.2f} s" for s in settle)
        print(f"  turns at {', '.join(f'{t[i]:.1f}' for i in turns)} s → settling: {shown}")
    else:
        print("  no turns")
        ok = max_e < SETTLE_E and sat == 0
        print(f"  static check (|e| < {SETTLE_E} all the time, no saturation): {'PASS' if ok else 'FAIL'}")
    print(f"  plot → {figure}")

    return {"rms": rms_e, "max": max_e, "sat": sat, "settle": settle,
            "dt_mean": statistics.mean(dts), "p95": p95, "dt_max": max(dts)}


def group_of(path):
    name = os.path.basename(path)
    if "stab-on" in name:
        return "stab on"
    if "stab-off" in name:
        return "stab off"
    return os.path.splitext(name)[0]


def mean(values):
    values = [v for v in values if v is not None and not math.isnan(v)]
    return statistics.mean(values) if values else float("nan")


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: python3 host/analyze_m5.py results/M5/b6-*.txt")

    groups = {}                                         # group name → list of result dicts
    for path in sys.argv[1:]:
        r = analyze(path)
        if r is not None:
            groups.setdefault(group_of(path), []).append(r)

    print()
    print(f"{'group':<12}{'n':>3}{'RMS e':>9}{'max|e|':>9}{'sat %':>8}{'settle s':>10}{'unsettled':>11}{'dt max ms':>11}")
    for g, rs in groups.items():
        settles = [s for r in rs for s in r["settle"]]
        unsettled = sum(1 for s in settles if s is None)
        settle_txt = f"{mean(settles):.2f}" if settles else "—"
        print(f"{g:<12}{len(rs):>3}{mean([r['rms'] for r in rs]):>9.3f}{mean([r['max'] for r in rs]):>9.3f}"
              f"{mean([r['sat'] for r in rs]):>8.1f}{settle_txt:>10}{unsettled:>11}{max(r['dt_max'] for r in rs):>11.2f}")


if __name__ == "__main__":
    main()
