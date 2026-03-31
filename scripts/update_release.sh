#!/usr/bin/env bash
set -euo pipefail

# Minimal update mechanism (manual-runbook automation)
# Usage:
#   sudo ./scripts/update_release.sh <version> <tarball_path>
# Example:
#   sudo ./scripts/update_release.sh 0.0.3 /tmp/qilab-0.0.3.tar.gz

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <version> <tarball_path>"
  echo "Example: $0 0.0.3 /tmp/qilab-0.0.3.tar.gz"
  exit 1
fi

VERSION="$1"
TARBALL_PATH="$2"

SERVICE_NAME="qilab"
INSTALL_ROOT="/opt/qilab"
RELEASES_DIR="$INSTALL_ROOT/releases"
CURRENT_LINK="$INSTALL_ROOT/current"
TARGET_DIR="$RELEASES_DIR/$VERSION/qilab-$VERSION"
CONFIG_DIR="/etc/qilab"
CONFIG_FILE="$CONFIG_DIR/nodes.yaml"
PROFILER_DIR="/var/log/qilab/profiler"

if [[ $EUID -ne 0 ]]; then
  echo "This script must be run as root (use sudo)."
  exit 1
fi

if [[ ! -f "$TARBALL_PATH" ]]; then
  echo "Tarball not found: $TARBALL_PATH"
  exit 1
fi

mkdir -p "$RELEASES_DIR"
mkdir -p "$CONFIG_DIR"
mkdir -p "$PROFILER_DIR"

PREV_TARGET=""
if [[ -L "$CURRENT_LINK" || -e "$CURRENT_LINK" ]]; then
  PREV_TARGET="$(readlink -f "$CURRENT_LINK" || true)"
fi

echo "Preparing release directory: $TARGET_DIR"
rm -rf "$TARGET_DIR"
mkdir -p "$TARGET_DIR"

echo "Extracting: $TARBALL_PATH"
tar -xzf "$TARBALL_PATH" -C "$TARGET_DIR" --strip-components=1

if [[ ! -f "$TARGET_DIR/qilab-$VERSION-linux-arm64" ]]; then
  echo "Update aborted: executable not found at $TARGET_DIR/qilab-$VERSION-linux-arm64"
  exit 1
fi

mv "$TARGET_DIR/qilab-$VERSION-linux-arm64" "$TARGET_DIR/qilab"
chmod +x "$TARGET_DIR/qilab"

if ! command -v file >/dev/null 2>&1; then
  echo "Update aborted: 'file' command is required for architecture validation."
  echo "Install it with: sudo apt-get install -y file"
  exit 1
fi

BINARY_INFO="$(file "$TARGET_DIR/qilab")"
echo "Binary info: $BINARY_INFO"
if ! echo "$BINARY_INFO" | grep -Eiq "aarch64|arm64"; then
  echo "Update aborted: binary is not ARM64/aarch64."
  exit 1
fi

# Keep config outside release directory so it can be updated independently.
# Seed config once (do not overwrite existing config).
if [[ ! -f "$CONFIG_FILE" ]]; then
  if [[ -f "$TARGET_DIR/config_example.yaml" ]]; then
    cp "$TARGET_DIR/config_example.yaml" "$CONFIG_FILE"
    echo "Initialized config: $CONFIG_FILE"
  else
    echo "Config file missing and no config_example.yaml found in package."
    echo "Create config manually at: $CONFIG_FILE"
  fi
fi

echo "Switching active symlink to: $TARGET_DIR"
ln -sfn "$TARGET_DIR" "$CURRENT_LINK"

echo "Restarting service: $SERVICE_NAME"
if systemctl restart "$SERVICE_NAME"; then
  echo "Update successful: $VERSION"
  echo "Config path (external): $CONFIG_FILE"
  echo "Set service env if needed: QILAB_CONFIG_DIR=$CONFIG_DIR"
  echo "Profiler dir (external): $PROFILER_DIR"
  echo "Set service env if needed: QILAB_PROFILER_DIR=$PROFILER_DIR"
  systemctl status "$SERVICE_NAME" --no-pager || true
  exit 0
fi

echo "Service restart failed. Rolling back..."
if [[ -n "$PREV_TARGET" && -d "$PREV_TARGET" ]]; then
  ln -sfn "$PREV_TARGET" "$CURRENT_LINK"
  systemctl restart "$SERVICE_NAME" || true
  echo "Rollback attempted to: $PREV_TARGET"
else
  echo "No valid previous target found. Manual recovery required."
fi

exit 1
