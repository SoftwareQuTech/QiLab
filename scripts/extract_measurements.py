#!/usr/bin/env python3
# Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
# SPDX-License-Identifier: BSD-3-Clause-Clear

"""Extract the sequence of measurement results (0/1) from a qilab log.

The log records measurements as lines such as:

    [13:11:28.913] [INFO]: Measured qubit 0 in Z-basis with result: 0

This script pulls out the 0/1 results in order and prints them.

Usage:
    python3 extract_measurements.py [LOGFILE]

Defaults to qilab.log in the current directory.
"""

import argparse
import bisect
import re

TIME_RE = re.compile(r"\[(\d{2}):(\d{2}):(\d{2})\.(\d{3})\]")
MEASURE_RE = re.compile(r"Measured qubit (\d+) in (\w)-basis with result:\s*([01])")
INIT_RE = re.compile(r"Initialized qubit (\d+)")
NOISE_RE = re.compile(
    r"Applied single-qubit noise: qubit=(\d+) "
    r"x_noise=([\d.eE+-]+) y_noise=([\d.eE+-]+) z_noise=([\d.eE+-]+)"
)
NOISE_DELAY_RE = re.compile(r"\$\$ TOTAL_TIME:\s*(\d+)")
SIM_TIME_RE = re.compile(
    r"\$\$ get_qubit_decoherence_probability: "
    r"Total_elapsed_sim_time_ns:\s*([\d.eE+-]+) ns"
)
GATE_RE = re.compile(r"Applying single-qubit gate: qubit=(\d+) angle=(\d+) axis=(\d+)")


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


def percentile(sorted_values, pct):
    """Return the pct-th percentile of an ascending-sorted list, using linear
    interpolation between adjacent ranks."""
    if not sorted_values:
        return None
    rank = (pct / 100.0) * (len(sorted_values) - 1)
    lo = int(rank)
    hi = min(lo + 1, len(sorted_values) - 1)
    frac = rank - lo
    return sorted_values[lo] * (1.0 - frac) + sorted_values[hi] * frac


def logged_value_stats(path, value_re, cutoff_pct=90.0):
    """Compute statistics of a numeric value extracted from log lines.

    value_re must have the numeric value as its first capture group. Returns a
    dict with:
      'count', 'mean', 'variance', 'std', 'min', 'max', 'median',
      'cutoff_pct'   - the percentile used as outlier cutoff,
      'cutoff'       - the value at that percentile,
      'cutoff_mean'  - mean of the samples <= cutoff,
      'cutoff_count' - number of samples <= cutoff,
    or None if no matching lines are found. Variance is the population
    variance. The cutoff mean discards the long tail (e.g. scheduling
    hiccups) that otherwise inflates the plain mean and variance.
    """
    values = []
    with open(path, "r", errors="replace") as f:
        for line in f:
            m = value_re.search(line)
            if m:
                values.append(float(m.group(1)))
    return _value_stats(values, cutoff_pct)


def _value_stats(values, cutoff_pct):
    """Build the statistics dict of logged_value_stats from a list of values
    (None if the list is empty)."""
    if not values:
        return None

    values = list(values)
    n = len(values)
    mean = sum(values) / n
    variance = sum((v - mean) ** 2 for v in values) / n
    values.sort()
    cutoff = percentile(values, cutoff_pct)
    kept = values[:bisect.bisect_right(values, cutoff)]
    return {
        "count": n,
        "mean": mean,
        "variance": variance,
        "std": variance ** 0.5,
        "min": values[0],
        "max": values[-1],
        "median": percentile(values, 50.0),
        "cutoff_pct": cutoff_pct,
        "cutoff": cutoff,
        "cutoff_mean": sum(kept) / len(kept) if kept else None,
        "cutoff_count": len(kept),
    }


def noise_delay_stats(path, cutoff_pct=90.0):
    """Statistics of the time (ns) spent applying noise, logged as:

        [10:01:52.642] [INFO]: $$ TOTAL_TIME: 188166

    See logged_value_stats for the returned dict.
    """
    return logged_value_stats(path, NOISE_DELAY_RE, cutoff_pct)


def elapsed_sim_time_stats(path, cutoff_pct=90.0, skip_gates=0,
                           skip_last_gates=0):
    """Statistics of the elapsed simulation time (ns) per init->measure pass.

    The decoherence-probability calculation logs a cumulative simulation-time
    counter (started at the beginning of the run and never reset):

        [10:28:51.788] [INFO]: $$ get_qubit_decoherence_probability: Total_elapsed_sim_time_ns: 212099312.000000 ns

    Several such lines occur within each init->measure pass (one per noise
    application). The elapsed sim time of one pass is therefore the difference
    between the last and the first counter value logged within that pass.
    Passes with fewer than two counter lines are skipped (no delta can be
    formed). Note: the counter lines carry no qubit id, so passes are tracked
    globally; with interleaved multi-qubit traffic the attribution is
    approximate.

    With skip_gates=N (N > 0) the delta instead starts at the counter value
    in effect when the N-th "Applying single-qubit gate" line after the init
    executes - e.g. skip_gates=2 measures from after the Z + SQRT_Y pair
    forming the Hadamard-equivalent state preparation. With
    skip_last_gates=M (M > 0) the delta ends at the counter in effect at the
    first gate of the pass's last M gates instead of at the measurement -
    e.g. skip_gates=2, skip_last_gates=2 measures only the wait between the
    state preparation and the pre-measurement basis rotation. The reference
    values are the counters logged with those gates' own noise applications
    (which precede the gates), so the idle time following a boundary gate is
    fully included. Passes with fewer than N+M gates are skipped.

    See logged_value_stats for the returned dict.
    """
    deltas = []
    counters = None       # counter values of the open pass, or None
    counter_at_gate = []  # counter in effect at each of the pass's gates
    with open(path, "r", errors="replace") as f:
        for line in f:
            if INIT_RE.search(line):
                counters = []
                counter_at_gate = []
                continue
            m = SIM_TIME_RE.search(line)
            if m:
                if counters is not None:
                    counters.append(float(m.group(1)))
                continue
            if GATE_RE.search(line):
                if counters is not None:
                    counter_at_gate.append(counters[-1] if counters else None)
                continue
            if MEASURE_RE.search(line):
                if counters is not None and \
                        len(counter_at_gate) >= skip_gates + skip_last_gates:
                    start = (counters[0] if counters else None) if skip_gates == 0 \
                        else counter_at_gate[skip_gates - 1]
                    end = (counters[-1] if counters else None) if skip_last_gates == 0 \
                        else counter_at_gate[len(counter_at_gate) - skip_last_gates]
                    if start is not None and end is not None and end >= start \
                            and not (skip_gates == 0 and skip_last_gates == 0
                                     and len(counters) < 2):
                        deltas.append(end - start)
                counters = None
                counter_at_gate = []
    return _value_stats(deltas, cutoff_pct)


def parse_time_ms(line):
    """Return the log timestamp of a line in milliseconds since midnight, or
    None if the line has no timestamp."""
    m = TIME_RE.search(line)
    if not m:
        return None
    h, mnt, s, ms = (int(g) for g in m.groups())
    return ((h * 60 + mnt) * 60 + s) * 1000 + ms


def touch_to_next_dephasing_stats(path, cutoff_pct=90.0, gate_n=2):
    """Statistics of the time (ns) from the qubit touch after the gate_n-th
    gate of a pass to the next application of the dephasing channel.

    After each gate the handler calls mark_touched() (logged as
    "$$ AFTER_TOUCH_TIME"), which resets the phase-flip model's reference
    time. The next get_qubit_decoherence_probability call advances the
    cumulative "Total_elapsed_sim_time_ns" counter by exactly
    (now - touch_time). The wanted interval therefore equals the increment
    between the counter line at the gate_n-th gate (logged just before the
    gate) and the first counter line after it - no use of the absolute
    AFTER_TOUCH_TIME timestamps is needed (they are on a different clock
    origin than the cumulative counter and cannot be differenced with it).

    Passes with fewer than gate_n gates, without a counter line before the
    gate, or without one after it are skipped.

    See logged_value_stats for the returned dict.
    """
    deltas = []
    counters = None   # counter values of the open pass, or None
    gate_counts = []  # len(counters) at the time of each gate line
    with open(path, "r", errors="replace") as f:
        for line in f:
            if INIT_RE.search(line):
                counters = []
                gate_counts = []
                continue
            m = SIM_TIME_RE.search(line)
            if m:
                if counters is not None:
                    counters.append(float(m.group(1)))
                continue
            if GATE_RE.search(line):
                if counters is not None:
                    gate_counts.append(len(counters))
                continue
            if MEASURE_RE.search(line):
                if counters is not None and len(gate_counts) >= gate_n:
                    cnt = gate_counts[gate_n - 1]  # counters seen up to that gate
                    if cnt >= 1 and len(counters) > cnt:
                        deltas.append(counters[cnt] - counters[cnt - 1])
                counters = None
                gate_counts = []
    return _value_stats(deltas, cutoff_pct)


def _interval_result_stats(pairs):
    """Summarize a list of (interval_ms, result) pairs: interval statistics
    plus measurement outcome counts."""
    n = len(pairs)
    intervals = sorted(p[0] for p in pairs)
    ones = sum(p[1] for p in pairs)
    return {
        "count": n,
        "mean": sum(intervals) / n,
        "median": percentile(intervals, 50.0),
        "min": intervals[0],
        "max": intervals[-1],
        "zeros": n - ones,
        "ones": ones,
    }


def init_measure_pass_stats(path, cutoff_pct=90.0, qubit=None, basis=None,
                            skip_gates=0, skip_last_gates=0):
    """Statistics of init->measure passes, with an outlier cutoff.

    Each measurement is paired with the most recent initialization of the same
    qubit, giving one pass per measurement with its elapsed time (ms) and its
    0/1 result. Timestamps that wrap past midnight are corrected by adding
    24h.

    With skip_gates=N (N > 0) the pass timing instead starts at the N-th
    "Applying single-qubit gate" line for that qubit after its init - e.g.
    skip_gates=2 starts the clock after the Z + SQRT_Y pair forming the
    Hadamard-equivalent state preparation. With skip_last_gates=M (M > 0) the
    timing ends at the first gate of the pass's last M gates instead of at
    the measurement - e.g. skip_gates=2, skip_last_gates=2 measures only the
    wait between the state preparation and the pre-measurement basis
    rotation. Passes with fewer than N+M gates are skipped.

    Returns (full, trimmed, cutoff) or None if no passes are found:
      full    - stats dict over all passes,
      trimmed - stats dict over the passes whose init->measure time is <= the
                cutoff_pct percentile (so outcome counts are also restricted
                to those passes),
      cutoff  - the interval value (ms) at that percentile.
    Each stats dict holds 'count', 'mean', 'median', 'min', 'max' for the
    interval, and 'zeros'/'ones' for the measurement results.
    """
    DAY_MS = 24 * 60 * 60 * 1000
    # qubit -> {'ts': init timestamp (ms), 'gate_ts': timestamps of the
    # pass's gate lines} - gate timestamps are buffered because the trailing
    # cut is only known once the measurement arrives.
    open_pass = {}
    pairs = []  # (interval_ms, result) per pass
    with open(path, "r", errors="replace") as f:
        for line in f:
            ts = parse_time_ms(line)
            mi = INIT_RE.search(line)
            if mi:
                open_pass[int(mi.group(1))] = {"ts": ts, "gate_ts": []}
                continue
            mg = GATE_RE.search(line)
            if mg:
                st = open_pass.get(int(mg.group(1)))
                if st:
                    st["gate_ts"].append(ts)
                continue
            mm = MEASURE_RE.search(line)
            if mm:
                q, b, r = int(mm.group(1)), mm.group(2), int(mm.group(3))
                if qubit is not None and q != qubit:
                    continue
                if basis is not None and b != basis:
                    continue
                st = open_pass.pop(q, None)
                if st is None:
                    continue  # measurement with no open pass
                gate_ts = st["gate_ts"]
                if len(gate_ts) < skip_gates + skip_last_gates:
                    continue  # too few gates for the requested cuts
                start_ts = st["ts"] if skip_gates == 0 else gate_ts[skip_gates - 1]
                end_ts = ts if skip_last_gates == 0 else gate_ts[len(gate_ts) - skip_last_gates]
                if start_ts is None or end_ts is None:
                    continue
                delta = end_ts - start_ts
                if delta < 0:  # crossed midnight
                    delta += DAY_MS
                pairs.append((delta, r))

    if not pairs:
        return None

    cutoff = percentile(sorted(p[0] for p in pairs), cutoff_pct)
    trimmed = [p for p in pairs if p[0] <= cutoff]
    return _interval_result_stats(pairs), _interval_result_stats(trimmed), cutoff


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
    parser.add_argument("-c", "--cutoff-percentile", type=float, default=90.0,
                        help="percentile used as outlier cutoff for the "
                             "trimmed noise-delay average (default: 90)")
    parser.add_argument("-g", "--skip-gates", type=int, default=0,
                        help="start each pass's timing after the Nth single-"
                             "qubit gate following init instead of at the "
                             "init itself; use 2 to skip the Z + SQRT_Y "
                             "Hadamard-equivalent state preparation (applies "
                             "to the init->measure and elapsed-sim-time "
                             "statistics; default: 0)")
    parser.add_argument("-l", "--skip-last-gates", type=int, default=0,
                        help="end each pass's timing at the first of its "
                             "last N single-qubit gates instead of at the "
                             "measurement; use 2 to exclude the Z + SQRT_Y "
                             "pre-measurement basis rotation (applies to the "
                             "init->measure and elapsed-sim-time statistics; "
                             "default: 0)")
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

    pass_start = (f"gate{a.skip_gates}" if a.skip_gates > 0 else "init")
    pass_end = (f"measure-{a.skip_last_gates}gates" if a.skip_last_gates > 0
                else "measure")
    passes = init_measure_pass_stats(path, cutoff_pct=a.cutoff_percentile,
                                     skip_gates=a.skip_gates,
                                     skip_last_gates=a.skip_last_gates)
    if passes:
        full, trimmed, cutoff = passes
        print(f"{pass_start.capitalize()}->{pass_end} passes "
              f"({full['count']} paired):")
        print(f"  average: {full['mean']:.3f} ms, median {full['median']:.1f} ms "
              f"(min {full['min']}, max {full['max']})")
        print(f"  results: 0 -> {full['zeros']}, 1 -> {full['ones']} "
              f"(fraction of 1s: {full['ones'] / full['count']:.3f})")
        print(f"  below p{a.cutoff_percentile:g} cutoff ({cutoff:.1f} ms), "
              f"{trimmed['count']} passes kept:")
        print(f"    average: {trimmed['mean']:.3f} ms, "
              f"median {trimmed['median']:.1f} ms "
              f"(min {trimmed['min']}, max {trimmed['max']})")
        print(f"    results: 0 -> {trimmed['zeros']}, 1 -> {trimmed['ones']} "
              f"(fraction of 1s: {trimmed['ones'] / trimmed['count']:.3f})")
    else:
        print(f"{pass_start.capitalize()}->{pass_end} time: "
              f"no matching passes found.")

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

    def print_ns_stats(title, s, missing):
        if not s:
            print(f"{title}: {missing}")
            return
        print(f"{title} ({s['count']} samples):")
        print(f"  average:  {s['mean']:.1f} ns ({s['mean'] / 1000:.3f} us)")
        print(f"  median:   {s['median']:.1f} ns ({s['median'] / 1000:.3f} us)")
        print(f"  variance: {s['variance']:.1f} ns^2 (std {s['std']:.1f} ns)")
        print(f"  min/max:  {s['min']:.0f} / {s['max']:.0f} ns")
        print(f"  average below p{s['cutoff_pct']:g} cutoff "
              f"({s['cutoff']:.0f} ns): {s['cutoff_mean']:.1f} ns "
              f"({s['cutoff_mean'] / 1000:.3f} us, "
              f"{s['cutoff_count']} samples kept)")

    print_ns_stats("Noise application delay",
                   noise_delay_stats(path, cutoff_pct=a.cutoff_percentile),
                   "no '$$ TOTAL_TIME' lines found.")
    print_ns_stats(f"Elapsed sim time per {pass_start}->{pass_end} pass",
                   elapsed_sim_time_stats(path, cutoff_pct=a.cutoff_percentile,
                                          skip_gates=a.skip_gates,
                                          skip_last_gates=a.skip_last_gates),
                   "no per-pass 'Total_elapsed_sim_time_ns' deltas found.")
    touch_gate = a.skip_gates if a.skip_gates > 0 else 2
    print_ns_stats(f"Touch after gate {touch_gate} -> next dephasing application",
                   touch_to_next_dephasing_stats(path,
                                                 cutoff_pct=a.cutoff_percentile,
                                                 gate_n=touch_gate),
                   "no matching counter increments found.")

    if a.sequence:
        print("Sequence:")
        print("".join(str(b) for b in seq))


if __name__ == "__main__":
    main()
