#!/usr/bin/env python3
"""Extract the sequence of measurement results (0/1) from a qilab log.

The log records measurements as lines such as:

    [13:11:28.913] [INFO]: Measured qubit 0 in Z-basis with result: 0

This script pulls out the 0/1 results in order and prints them.

Usage:
    python3 extract_measurements.py [LOGFILE]

Defaults to qilab.log in the current directory.
"""

import argparse
import re

TIME_RE = re.compile(r"\[(\d{2}):(\d{2}):(\d{2})\.(\d{3})\]")
MEASURE_RE = re.compile(r"Measured qubit (\d+) in (\w)-basis with result:\s*([01])")
INIT_RE = re.compile(r"Initialized qubit (\d+)")
NOISE_RE = re.compile(
    r"Applied single-qubit noise: qubit=(\d+) "
    r"x_noise=([\d.eE+-]+) y_noise=([\d.eE+-]+) z_noise=([\d.eE+-]+)"
)


def average_pass_noise(path, qubit=None, basis=None):
    """Average the total noise probability over each init->measure pass.

    A "pass" is one initialization of a qubit up to its next measurement. The
    noise probabilities logged within a pass are per-event *error*
    probabilities and are NOT additive. To combine them we convert each to a
    probability of *no* error (1 - p), multiply those together across the
    events in the pass, and convert back:

        P_error_total(pass) = 1 - product(1 - p_i)

    This is done per Pauli axis (X, Y, Z) and combined across all axes into an
    overall total for the pass. Those per-pass totals are then averaged across
    all passes.

    Returns a dict: qubit -> {
        'passes': number of completed init->measure passes,
        'avg_x', 'avg_y', 'avg_z': mean per-axis total error probability,
        'avg_overall': mean overall total error probability,
    }
    """
    # qubit -> [no_err_x, no_err_y, no_err_z] for the current, open pass
    open_pass = {}
    # qubit -> list of (x, y, z, overall) totals, one per completed pass
    pass_totals = {}

    with open(path, "r", errors="replace") as f:
        for line in f:
            mi = INIT_RE.search(line)
            if mi:
                q = int(mi.group(1))
                if qubit is None or q == qubit:
                    open_pass[q] = [1.0, 1.0, 1.0]  # start/restart the pass
                continue

            mn = NOISE_RE.search(line)
            if mn:
                q = int(mn.group(1))
                if q in open_pass:
                    px, py, pz = (float(mn.group(i)) for i in (2, 3, 4))
                    open_pass[q][0] *= (1.0 - px)
                    open_pass[q][1] *= (1.0 - py)
                    open_pass[q][2] *= (1.0 - pz)
                continue

            mm = MEASURE_RE.search(line)
            if mm:
                q, b = int(mm.group(1)), mm.group(2)
                if qubit is not None and q != qubit:
                    continue
                if basis is not None and b != basis:
                    continue
                if q not in open_pass:
                    continue  # measurement with no preceding init
                nx, ny, nz = open_pass.pop(q)
                pass_totals.setdefault(q, []).append(
                    (1.0 - nx, 1.0 - ny, 1.0 - nz, 1.0 - nx * ny * nz)
                )

    result = {}
    for q, totals in pass_totals.items():
        m = len(totals)
        result[q] = {
            "passes": m,
            "avg_x": sum(t[0] for t in totals) / m,
            "avg_y": sum(t[1] for t in totals) / m,
            "avg_z": sum(t[2] for t in totals) / m,
            "avg_overall": sum(t[3] for t in totals) / m,
        }
    return result


def parse_time_ms(line):
    """Return the log timestamp of a line in milliseconds since midnight, or
    None if the line has no timestamp."""
    m = TIME_RE.search(line)
    if not m:
        return None
    h, mnt, s, ms = (int(g) for g in m.groups())
    return ((h * 60 + mnt) * 60 + s) * 1000 + ms


def average_init_to_measure_ms(path, qubit=None, basis=None):
    """Average elapsed time (ms) between a qubit's initialization and its next
    measurement.

    Each measurement is paired with the most recent initialization of the same
    qubit. Returns (average_ms, intervals) where intervals is the list of
    per-measurement elapsed times. Timestamps that wrap past midnight are
    corrected by adding 24h.
    """
    DAY_MS = 24 * 60 * 60 * 1000
    last_init = {}  # qubit -> init timestamp (ms)
    intervals = []
    with open(path, "r", errors="replace") as f:
        for line in f:
            ts = parse_time_ms(line)
            mi = INIT_RE.search(line)
            if mi:
                last_init[int(mi.group(1))] = ts
                continue
            mm = MEASURE_RE.search(line)
            if mm:
                q, b = int(mm.group(1)), mm.group(2)
                if qubit is not None and q != qubit:
                    continue
                if basis is not None and b != basis:
                    continue
                init_ts = last_init.get(q)
                if init_ts is None or ts is None:
                    continue
                delta = ts - init_ts
                if delta < 0:  # crossed midnight
                    delta += DAY_MS
                intervals.append(delta)
    avg = sum(intervals) / len(intervals) if intervals else None
    return avg, intervals


def extract_results(path, qubit=None, basis=None):
    """Return the ordered list of measurement results (as ints 0/1).

    Optionally filter by qubit index and/or measurement basis.
    """
    results = []
    with open(path, "r", errors="replace") as f:
        for line in f:
            m = MEASURE_RE.search(line)
            if not m:
                continue
            q, b, r = int(m.group(1)), m.group(2), int(m.group(3))
            if qubit is not None and q != qubit:
                continue
            if basis is not None and b != basis:
                continue
            results.append(r)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logfile", nargs="?", default="qilab.log",
                        help="path to the qilab log (default: qilab.log)")
    parser.add_argument("-s", "--sequence", action="store_true",
                        help="print the full 0/1 measurement sequence")
    a = parser.parse_args()
    path = a.logfile

    seq = extract_results(path)
    n = len(seq)
    print(f"Log file: {path}")
    print(f"Total measurements extracted: {n}")
    if n == 0:
        print("No measurement results found.")
        return

    ones = sum(seq)
    print(f"Result counts: 0 -> {n - ones}, 1 -> {ones}")
    print(f"Fraction of 1s: {ones / n:.3f}")
    print(f"Fraction of 0s: {(n - ones) / n:.3f}")

    avg, intervals = average_init_to_measure_ms(path)
    if intervals:
        print(f"Init->measure intervals: {len(intervals)} paired")
        print(f"Average init->measure time: {avg:.3f} ms "
              f"(min {min(intervals)}, max {max(intervals)})")
    else:
        print("Init->measure time: no init/measure pairs found.")

    noise = average_pass_noise(path)
    if noise:
        print("Average total noise probability per init->measure pass "
              "(combined via 1 - product(1 - p)):")
        for q in sorted(noise):
            t = noise[q]
            print(f"  qubit {q} ({t['passes']} passes): "
                  f"X={t['avg_x']:.6f} Y={t['avg_y']:.6f} Z={t['avg_z']:.6f} "
                  f"overall={t['avg_overall']:.6f}")
    else:
        print("Average noise probability: no init->measure passes found.")

    if a.sequence:
        print("Sequence:")
        print("".join(str(b) for b in seq))


if __name__ == "__main__":
    main()
