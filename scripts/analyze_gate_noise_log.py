#!/usr/bin/env python3
# Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
# SPDX-License-Identifier: BSD-3-Clause-Clear

"""Analyze measurement outcomes and channel noise in a qilab log.

Parses log lines such as:

    [13:11:28.913] [INFO]: Measured qubit 0 in Z-basis with result: 0
    [13:11:28.913] [INFO]: Applied single-qubit noise: qubit=0 x_noise=0.000000 y_noise=0.000000 z_noise=0.000000

and reports, per qubit:
  - the average measurement outcome (fraction of 1s), and
  - the average Pauli channel noise probabilities, their total, and the
    corresponding average single-qubit gate fidelity.

For a single-qubit Pauli channel
    E(rho) = (1 - px - py - pz) rho + px X rho X + py Y rho Y + pz Z rho Z
the average gate fidelity is
    F = 1 - (2/3) (px + py + pz)
(uniform px = py = pz = p/3 recovers the depolarizing relation F = 1 - 2p/3).
F is linear in the probabilities, so the mean per-application fidelity equals
the fidelity of the mean channel.

With --check-composition the script additionally verifies the noise
composition logged by the simulator:

    Probabilities before composing: q1_x=... q1_y=... q1_z=... q2_x=... q2_y=... q2_z=...
    Composed single-qubit noise channel: q_x=... q_y=... q_z=...

Each composed line is recomputed from the preceding before-composing line
using the single-qubit Pauli channel composition rule (products of Paulis:
X.Y ~ Z etc., identity weight = 1 - qx - qy - qz):

    q_x = i1 x2 + x1 i2 + y1 z2 + z1 y2
    q_y = i1 y2 + y1 i2 + x1 z2 + z1 x2
    q_z = i1 z2 + z1 i2 + x1 y2 + y1 x2

and compared against the logged values within a tolerance.

Usage:
    python3 analyze_gate_noise_log.py [LOGFILE] [--check-composition]

Defaults to qilab.log in the current directory.
"""

import argparse
import re

MEASURE_RE = re.compile(r"Measured qubit (\d+) in (\w)-basis with result:\s*([01])")
NOISE_RE = re.compile(
    r"Applied single-qubit noise: qubit=(\d+) "
    r"x_noise=([\d.eE+-]+) y_noise=([\d.eE+-]+) z_noise=([\d.eE+-]+)"
)
COMPOSE_IN_RE = re.compile(
    r"Probabilities before composing: "
    r"q1_x=([\d.eE+-]+) q1_y=([\d.eE+-]+) q1_z=([\d.eE+-]+) "
    r"q2_x=([\d.eE+-]+) q2_y=([\d.eE+-]+) q2_z=([\d.eE+-]+)"
)
COMPOSE_OUT_RE = re.compile(
    r"Composed single-qubit noise channel: "
    r"q_x=([\d.eE+-]+) q_y=([\d.eE+-]+) q_z=([\d.eE+-]+)"
)


def pauli_channel_fidelity(px, py, pz):
    """Average single-qubit gate fidelity of a Pauli channel (px, py, pz)."""
    return 1.0 - (2.0 / 3.0) * (px + py + pz)


def compose_pauli_channels(q1, q2):
    """Compose two single-qubit Pauli channels (applied q1 then q2).

    q1 and q2 are (px, py, pz) tuples; returns the composed (qx, qy, qz).
    Mirrors StimAdapterNVCenters::compose_single_qubit_noise_channel.
    """
    x1, y1, z1 = q1
    x2, y2, z2 = q2
    i1 = 1.0 - (x1 + y1 + z1)
    i2 = 1.0 - (x2 + y2 + z2)
    return (
        i1 * x2 + x1 * i2 + y1 * z2 + z1 * y2,
        i1 * y2 + y1 * i2 + x1 * z2 + z1 * x2,
        i1 * z2 + z1 * i2 + x1 * y2 + y1 * x2,
    )


def parse_log(path):
    """Collect per-qubit measurement results and noise applications.

    Returns (measurements, noise) where
      measurements: qubit -> list of 0/1 results,
      noise:        qubit -> list of (px, py, pz) tuples.
    """
    measurements = {}
    noise = {}
    with open(path, "r", errors="replace") as f:
        for line in f:
            mm = MEASURE_RE.search(line)
            if mm:
                q, r = int(mm.group(1)), int(mm.group(3))
                measurements.setdefault(q, []).append(r)
                continue
            mn = NOISE_RE.search(line)
            if mn:
                q = int(mn.group(1))
                probs = tuple(float(mn.group(i)) for i in (2, 3, 4))
                noise.setdefault(q, []).append(probs)
    return measurements, noise


def check_composition(path, tolerance=1e-5):
    """Verify the logged composed noise channels against a recomputation.

    Pairs each "Composed single-qubit noise channel" line with the most
    recent "Probabilities before composing" line. Returns a dict with
      'checked'    - number of composed lines verified,
      'mismatches' - list of (line_number, logged, expected) for lines whose
                     recomputed channel deviates more than tolerance in any
                     component,
      'max_error'  - largest absolute per-component deviation seen,
    or None if no composition lines are found.
    """
    checked = 0
    mismatches = []
    max_error = 0.0
    inputs = None  # most recent before-composing probabilities
    with open(path, "r", errors="replace") as f:
        for lineno, line in enumerate(f, 1):
            mi = COMPOSE_IN_RE.search(line)
            if mi:
                vals = [float(g) for g in mi.groups()]
                inputs = (tuple(vals[0:3]), tuple(vals[3:6]))
                continue
            mo = COMPOSE_OUT_RE.search(line)
            if mo:
                if inputs is None:
                    continue  # composed line with no preceding inputs
                logged = tuple(float(g) for g in mo.groups())
                expected = compose_pauli_channels(*inputs)
                error = max(abs(l - e) for l, e in zip(logged, expected))
                max_error = max(max_error, error)
                checked += 1
                if error > tolerance:
                    mismatches.append((lineno, logged, expected))
                inputs = None  # one composed line per before-composing line

    if checked == 0:
        return None
    return {"checked": checked, "mismatches": mismatches, "max_error": max_error}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logfile", nargs="?", default="qilab.log",
                        help="path to the qilab log (default: qilab.log)")
    parser.add_argument("--check-composition", action="store_true",
                        help="verify 'Composed single-qubit noise channel' "
                             "lines against a recomputation from the "
                             "preceding 'Probabilities before composing' "
                             "lines")
    parser.add_argument("--tolerance", type=float, default=1e-5,
                        help="maximum allowed per-component deviation for "
                             "the composition check (default: 1e-5; the "
                             "logged values are rounded to 6 decimals)")
    a = parser.parse_args()

    measurements, noise = parse_log(a.logfile)
    print(f"Log file: {a.logfile}")

    if not measurements:
        print("No measurement results found.")
    for q in sorted(measurements):
        results = measurements[q]
        n = len(results)
        ones = sum(results)
        print(f"Qubit {q} measurements ({n} total):")
        print(f"  counts:  0 -> {n - ones}, 1 -> {ones}")
        print(f"  average: {ones / n:.6f} (fraction of 1s)")

    if not noise:
        print("No single-qubit noise applications found.")
    for q in sorted(noise):
        probs = noise[q]
        n = len(probs)
        avg_x = sum(p[0] for p in probs) / n
        avg_y = sum(p[1] for p in probs) / n
        avg_z = sum(p[2] for p in probs) / n
        avg_total = avg_x + avg_y + avg_z
        print(f"Qubit {q} channel noise ({n} applications):")
        print(f"  average probabilities: x={avg_x:.6f} y={avg_y:.6f} "
              f"z={avg_z:.6f}")
        print(f"  average total probability: {avg_total:.6f}")
        print(f"  average single-qubit gate fidelity: "
              f"{pauli_channel_fidelity(avg_x, avg_y, avg_z):.6f}")

    if a.check_composition:
        result = check_composition(a.logfile, tolerance=a.tolerance)
        if result is None:
            print("Composition check: no composition lines found.")
        else:
            print(f"Composition check ({result['checked']} compositions, "
                  f"tolerance {a.tolerance:g}):")
            print(f"  max per-component deviation: {result['max_error']:.2e}")
            if result["mismatches"]:
                print(f"  MISMATCHES: {len(result['mismatches'])}")
                for lineno, logged, expected in result["mismatches"][:10]:
                    print(f"    line {lineno}: logged=({logged[0]:.6f}, "
                          f"{logged[1]:.6f}, {logged[2]:.6f}) "
                          f"expected=({expected[0]:.6f}, {expected[1]:.6f}, "
                          f"{expected[2]:.6f})")
                if len(result["mismatches"]) > 10:
                    print(f"    ... and {len(result['mismatches']) - 10} more")
            else:
                print("  all compositions consistent.")


if __name__ == "__main__":
    main()
