#!/usr/bin/env bash
set -euo pipefail

# Local release MVP:
# 1) Configure + build Release
# 2) Package binary and minimal runtime files into tar.gz
#
# Usage:
#   ./scripts/release_local.sh 0.0.3

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <version>"
  echo "Example: $0 0.0.3"
  exit 1
fi

VERSION="$1"
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build-release"
PACKAGE_DIR="$PROJECT_ROOT/dist/qilab-${VERSION}"
ARCHIVE_PATH="$PROJECT_ROOT/dist/qilab-${VERSION}.tar.gz"

mkdir -p "$PROJECT_ROOT/dist"

cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --target qilab -- -j"$(nproc)"

rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR"

cp "$BUILD_DIR/qilab" "$PACKAGE_DIR/"
cp "$PROJECT_ROOT/README.md" "$PACKAGE_DIR/"

if [[ -f "$PROJECT_ROOT/docs/config_example.yaml" ]]; then
  cp "$PROJECT_ROOT/docs/config_example.yaml" "$PACKAGE_DIR/config_example.yaml"
fi

tar -czf "$ARCHIVE_PATH" -C "$PROJECT_ROOT/dist" "qilab-${VERSION}"

echo "Created: $ARCHIVE_PATH"
