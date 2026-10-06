#!/usr/bin/env python3
# Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
# SPDX-License-Identifier: BSD-3-Clause-Clear

"""Stamp a version onto docs/config.schema.json for release packaging.

Usage: stamp_schema_version.py <version> <output_path> [schema_path]
"""
import json
import sys


def main():
    if len(sys.argv) < 3:
        print(f"usage: {sys.argv[0]} <version> <output_path> [schema_path]", file=sys.stderr)
        return 1

    version = sys.argv[1]
    output_path = sys.argv[2]
    schema_path = sys.argv[3] if len(sys.argv) > 3 else "docs/config.schema.json"

    with open(schema_path) as f:
        schema = json.load(f)
    schema["x-qilab-schema-version"] = version
    with open(output_path, "w") as f:
        json.dump(schema, f, indent=2)
        f.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
