"""Plot the M2 roll-filter logs as SVG figures (no extra packages needed).

Run from anywhere, e.g.  python3 host/plot_m2.py  or  python3 plot_m2.py
Reads results/M2/*.txt and writes figures/M2/m2-*.svg.
"""
import math
import os

# Paths are found from this file's location (host/ is one level below the project root).
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULTS = os.path.join(ROOT, "results", "M2")
FIGURES = os.path.join(ROOT, "figures", "M2")

# Colours: chart surface, text, grid, then one fixed colour per series.
SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_SOFT = "#52514e"
GRID = "#e4e3df"
SERIES = [  # (column, label, colour) in the log's column order
    (3, "accel only", "#eb6834"),
    (2, "gyro only", "#2a78d6"),
    (4, "filtered", "#1baf7a"),
]
INVALID_FILL = "#eeedea"


def load(name):
    """Return filter rows (time, dt, gyro, accel, filtered) and any INVALID/VALID times."""
    rows, invalid, valid = [], [], []
    for line in open(os.path.join(RESULTS, name)):
        parts = line.strip().split(",")
        if len(parts) >= 2 and parts[1] == "INVALID":
            invalid.append(float(parts[0]))
        elif len(parts) >= 2 and parts[1] == "VALID":
            valid.append(float(parts[0]))
        elif len(parts) == 5 and parts[0][:1].isdigit() and "." in parts[0]:
            rows.append(tuple(float(p) for p in parts))
    return rows, invalid, valid


def nice_ticks(lo, hi, count=5):
    """Round tick values that cover lo..hi."""
    raw = (hi - lo) / count
    step = 10 ** math.floor(math.log10(raw))
    for m in (1, 2, 5, 10):
        if raw <= m * step:
            step *= m
            break
    first = math.ceil(lo / step) * step
    ticks = []
    t = first
    while t <= hi + 1e-9:
        ticks.append(round(t, 6))
        t += step
    return ticks


def panel(x, y, w, h, rows, title, y_range, t_range=None, columns=SERIES, bands=(), notes=()):
    """One chart: time on x, degrees on y. Returns SVG text."""
    t0, t1 = t_range or (rows[0][0], rows[-1][0])
    y0, y1 = y_range
    sx = lambda t: x + (t - t0) / (t1 - t0) * w
    sy = lambda v: y + h - (v - y0) / (y1 - y0) * h
    out = [f'<text x="{x}" y="{y - 10}" font-size="13" font-weight="600" fill="{TEXT}">{title}</text>']

    # Shaded time bands (e.g. sensor unplugged), drawn under everything.
    for b0, b1, label in bands:
        out.append(f'<rect x="{sx(b0):.1f}" y="{y}" width="{sx(b1) - sx(b0):.1f}" height="{h}" fill="{INVALID_FILL}"/>')
        out.append(f'<text x="{(sx(b0) + sx(b1)) / 2:.1f}" y="{y + 16}" font-size="11" text-anchor="middle" fill="{TEXT_SOFT}">{label}</text>')

    # Hairline grid and axis labels.
    for v in nice_ticks(y0, y1):
        out.append(f'<line x1="{x}" x2="{x + w}" y1="{sy(v):.1f}" y2="{sy(v):.1f}" stroke="{GRID}" stroke-width="1"/>')
        out.append(f'<text x="{x - 6}" y="{sy(v) + 4:.1f}" font-size="11" text-anchor="end" fill="{TEXT_SOFT}">{v:g}°</text>')
    for t in nice_ticks(t0, t1, 6):
        out.append(f'<text x="{sx(t):.1f}" y="{y + h + 16}" font-size="11" text-anchor="middle" fill="{TEXT_SOFT}">{t:g} s</text>')

    # Lines: accel first (noisiest, at the back), filtered last (on top).
    for col, _, colour in columns:
        pts = [(sx(r[0]), sy(min(max(r[col], y0), y1))) for r in rows if t0 <= r[0] <= t1]
        # Break the line where data is missing (gap longer than 0.5 s).
        path, last_t = [], None
        for (px, py), r in zip(pts, [r for r in rows if t0 <= r[0] <= t1]):
            cmd = "M" if last_t is None or r[0] - last_t > 0.5 else "L"
            path.append(f"{cmd}{px:.1f},{py:.1f}")
            last_t = r[0]
        out.append(f'<path d="{" ".join(path)}" fill="none" stroke="{colour}" stroke-width="2" '
                   f'stroke-linejoin="round" stroke-linecap="round"/>')

    for t, v, text in notes:
        out.append(f'<text x="{sx(t):.1f}" y="{sy(v):.1f}" font-size="11" fill="{TEXT}">{text}</text>')
    return "\n".join(out)


def legend(x, y, columns=SERIES):
    out, cx = [], x
    for _, label, colour in columns:
        out.append(f'<line x1="{cx}" x2="{cx + 18}" y1="{y}" y2="{y}" stroke="{colour}" stroke-width="3" stroke-linecap="round"/>')
        out.append(f'<text x="{cx + 24}" y="{y + 4}" font-size="12" fill="{TEXT}">{label}</text>')
        cx += 24 + 8 * len(label) + 24
    return "\n".join(out)


def save(name, width, height, title, subtitle, body):
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
           f'viewBox="0 0 {width} {height}" font-family="-apple-system, Helvetica, Arial, sans-serif">\n'
           f'<rect width="100%" height="100%" fill="{SURFACE}"/>\n'
           f'<text x="24" y="30" font-size="16" font-weight="700" fill="{TEXT}">{title}</text>\n'
           f'<text x="24" y="50" font-size="12" fill="{TEXT_SOFT}">{subtitle}</text>\n'
           f'{body}\n</svg>\n')
    path = os.path.join(FIGURES, name)
    with open(path, "w") as f:
        f.write(svg)
    print("wrote", path)


def tau_comparison():
    """Step 6: same test with three tau values, one panel each."""
    runs = [("0.1", "imu-step6-roll-filter-tau0.1.txt"),
            ("0.5", "imu-step6-roll-filter-tau0.5.txt"),
            ("2.0", "imu-step6-roll-filter-tau2.0.txt")]
    parts = [legend(24, 76)]
    for i, (tau, name) in enumerate(runs):
        rows, _, _ = load(name)
        parts.append(panel(70, 120 + i * 230, 840, 170, rows, f"tau = {tau} s", (-30, 55)))
    save("m2-step6-tau-comparison.svg", 940, 820, "M2 step 6: roll, complementary filter with three tau values",
         "Still → tilt → slide → still. Before the low-pass filter. Roll in degrees.", "\n".join(parts))


def slide_before_after():
    """Slide with tau 0.5, before and after turning on the sensor low-pass filter."""
    before, _, _ = load("imu-step6-roll-filter-tau0.5.txt")
    after, _, _ = load("imu-step7-A-slide-lowpass.txt")
    parts = [legend(24, 76),
             panel(70, 120, 840, 200, before, "Before: sensor low-pass filter off (step 6, tau 0.5 s)", (-30, 15), (30, 46)),
             panel(70, 400, 840, 200, after, "After: low-pass filter on, CONFIG = 0x04 (step 7 test A, tau 0.5 s)", (-30, 15), (30, 46))]
    save("m2-step7-slide-lowpass-before-after.svg", 940, 650, "M2 step 7: sliding the board, before and after the low-pass filter",
         "Same y scale in both panels. Slides were done by hand, so they are not identical.", "\n".join(parts))


def bias_error():
    """Step 7 test C: the bias is deliberately wrong by +1 degree/s."""
    rows, _, _ = load("imu-step7-C-bias-error-1dps.txt")
    zoom_cols = [SERIES[0], SERIES[2]]  # accel and filtered only
    parts = [legend(24, 76),
             panel(70, 120, 840, 200, rows, "Full scale: gyro only climbs about 1° every second", (0, 35),
                   notes=[(27, 22, "gyro only: +1.002°/s")]),
             panel(70, 400, 840, 160, rows, "Zoomed in: filtered settles 0.5° above accel (error × tau = 1 × 0.5)", (1.5, 3.0),
                   columns=zoom_cols, notes=[(36, 2.85, "filtered − accel = 0.500°")])]
    save("m2-step7-bias-error-test.svg", 940, 610, "M2 step 7 test C: bias deliberately wrong by +1°/s, board still",
         "tau 0.5 s. The filter turns a growing gyro error into a small fixed one.", "\n".join(parts))


def unplug():
    """Step 7 test E: SDA pulled, then pushed back in."""
    rows, invalid, valid = load("imu-step7-E-unplug-retry.txt")
    bands = [(invalid[0], valid[-1], "INVALID: SDA unplugged, no angles reported")]
    parts = [legend(24, 76),
             panel(70, 120, 840, 220, rows, "Roll during the unplug test (board still)", (1.5, 3.0), bands=bands)]
    save("m2-step7-unplug-recovery.svg", 940, 400, "M2 step 7 test E: sensor unplugged, loop keeps running",
         f"INVALID at {invalid[0]:.2f} s, VALID again at {valid[-1]:.2f} s; angles restart from accel.", "\n".join(parts))


if __name__ == "__main__":
    os.makedirs(FIGURES, exist_ok=True)
    tau_comparison()
    slide_before_after()
    bias_error()
    unplug()
