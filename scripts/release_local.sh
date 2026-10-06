#!/usr/bin/env bash
# Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
# SPDX-License-Identifier: BSD-3-Clause-Clear

set -euo pipefail

# Local release build — mirrors the GitLab CI `release_prepare` job exactly
# (see .gitlab-ci.yml).
#
# Produces the same artifacts as CI:
#   - dist/qilab-<TAG>-linux-arm64.tar.gz   (aarch64 binary + README + config
#                                            + version-stamped config schema)
#   - dist-update/qilab-update-<TAG>.tar.gz (systemd unit + update script)
#   - dist/release_notes.md                 (extracted from CHANGELOG.md)
#
# The binary is cross-compiled for aarch64/arm64, identical to CI.
#
# Usage:
#   ./scripts/release_local.sh <tag>
# Example (tag must match the git tag form used in CI, including leading "v"):
#   ./scripts/release_local.sh v0.0.3

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <tag>"
  echo "Example: $0 v0.0.3"
  exit 1
fi

# In CI this comes from the pipeline; locally it is the script argument.
CI_COMMIT_TAG="$1"
VERSION="${CI_COMMIT_TAG#v}"

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

# Clean prior outputs so the build is reproducible (CI always starts fresh).
rm -rf build-release dist dist-update

cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/aarch64-linux-gnu-gcc -DCMAKE_CXX_COMPILER=/usr/bin/aarch64-linux-gnu-g++ -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DQILAB_CI_TAG=${CI_COMMIT_TAG} -DBUILD_TESTING=OFF
cmake --build build-release --target qilab --parallel "$(nproc)"
test -f build-release/qilab
file build-release/qilab | tee build-release/binary_arch.txt
grep -E "aarch64|ARM aarch64" build-release/binary_arch.txt

mkdir -p "dist/qilab-${VERSION}/"
mkdir -p "dist-update/qilab-update-${VERSION}/deploy/systemd" "dist-update/qilab-update-${VERSION}/scripts"

cp build-release/qilab "dist/qilab-${VERSION}/"
mv "dist/qilab-${VERSION}/qilab" "dist/qilab-${VERSION}/qilab-${VERSION}-linux-arm64"

cp README.md "dist/qilab-${VERSION}/"
# License texts: QiLab (BSD-3-Clause-Clear) and Stim, which is statically
# linked into the binary (Apache-2.0). Both licenses require these to ship.
cp LICENSE "dist/qilab-${VERSION}/LICENSE"
cp build-release/_deps/stim-src/LICENSE "dist/qilab-${VERSION}/LICENSE.stim"
cp deploy/systemd/qilab.service "dist-update/qilab-update-${VERSION}/deploy/systemd/qilab.service"
cp scripts/update_release.sh "dist-update/qilab-update-${VERSION}/scripts/update_release.sh"
chmod +x "dist-update/qilab-update-${VERSION}/scripts/update_release.sh"
if [ -f docs/config_example.yaml ]; then cp docs/config_example.yaml "dist/qilab-${VERSION}/config_example.yaml"; fi

# Ship the config schema alongside the binary, stamped with this release's
# version, so qilab_web_control can consume it straight from the deployed
# release directory (see docs/config_guide.md).
python3 scripts/stamp_schema_version.py "${VERSION}" "dist/qilab-${VERSION}/config.schema.json"
tar -czf "dist/qilab-${CI_COMMIT_TAG}-linux-arm64.tar.gz" -C dist "qilab-${VERSION}"
tar -czf "dist-update/qilab-update-${CI_COMMIT_TAG}.tar.gz" -C dist-update "qilab-update-${VERSION}"

awk -v v="$VERSION" '
  index($0, "## [" v "]") == 1 { in_section=1 }
  in_section && index($0, "## [") == 1 && index($0, "## [" v "]") != 1 { exit }
  in_section { print }
' CHANGELOG.md > dist/release_notes.md

test -s dist/release_notes.md || { echo "Missing changelog section for ${VERSION} in CHANGELOG.md"; exit 1; }

echo
echo "Created:"
echo "  dist/qilab-${CI_COMMIT_TAG}-linux-arm64.tar.gz"
echo "  dist-update/qilab-update-${CI_COMMIT_TAG}.tar.gz"
echo "  dist/release_notes.md"
