#!/usr/bin/env bash
# Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
# SPDX-License-Identifier: BSD-3-Clause-Clear

# Mirror every gate a vX.Y.Z tag has to clear in .gitlab-ci.yml, locally.
#
# Usage: .claude/skills/bump-version/check_release.sh 0.4.3 [--quick]
#
#   --quick   skip the build/test/packaging checks (stim compiles twice, so a
#             full run is slow). Use while iterating on the changelog, then do
#             a full run before reporting to the user.
#
# Each check names the CI job it stands in for, so a failure here points at the
# job that would have failed after the tag was already pushed.

set -uo pipefail

TARGET="${1:-}"
if [[ -z "$TARGET" ]]; then
  echo "Usage: $0 <version> [--quick]   (e.g. 0.4.3, no leading v)"
  exit 2
fi
QUICK=0
[[ "${2:-}" == "--quick" ]] && QUICK=1

PROJECT_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$PROJECT_ROOT" || exit 2

FAILURES=0
WARNINGS=0
pass() { printf '  \033[32mPASS\033[0m  %s\n' "$1"; }
fail() { printf '  \033[31mFAIL\033[0m  %s\n' "$1"; FAILURES=$((FAILURES + 1)); }
warn() { printf '  \033[33mWARN\033[0m  %s\n' "$1"; WARNINGS=$((WARNINGS + 1)); }
skip() { printf '  \033[36mSKIP\033[0m  %s\n' "$1"; }
step() { printf '\n%s\n' "$1"; }

# Extract MAJOR.MINOR.PATCH by NAME, not by position: keying on order would break
# silently if the three lines are ever reordered.
version_from() { # <file> <sed-regex with \1=name \2=value>
  local file="$1" regex="$2" major minor patch
  major="$(sed -nE "$regex" "$file" | sed -nE 's/^MAJOR=([0-9]+)$/\1/p' | head -1)"
  minor="$(sed -nE "$regex" "$file" | sed -nE 's/^MINOR=([0-9]+)$/\1/p' | head -1)"
  patch="$(sed -nE "$regex" "$file" | sed -nE 's/^PATCH=([0-9]+)$/\1/p' | head -1)"
  printf '%s.%s.%s' "${major:-?}" "${minor:-?}" "${patch:-?}"
}
CMAKE_RE='s/^set\(QILAB_VERSION_(MAJOR|MINOR|PATCH)[[:space:]]+([0-9]+)\).*/\1=\2/p'
CPP_RE='s/^constexpr int VERSION_(MAJOR|MINOR|PATCH) = ([0-9]+);.*/\1=\2/p'

# --- tag shape --------------------------------------------------------------
# Not a CI job. There is no `workflow:` rule, so any tag starts a pipeline --
# but CMakeLists.txt only accepts vX.Y.Z / X.Y.Z and SILENTLY falls back to the
# hardcoded QILAB_VERSION_* otherwise, producing a binary that misreports its
# own version. Worth failing loudly on here.
step "tag shape (CMakeLists.txt QILAB_CI_TAG regex)"
if [[ "$TARGET" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  pass "v$TARGET matches ^v?[0-9]+\\.[0-9]+\\.[0-9]+\$"
else
  fail "'$TARGET' is not X.Y.Z -- CMake would silently keep the hardcoded version"
fi

# --- CMake version ----------------------------------------------------------
# Not CI-gated (CI overrides from the tag), but it is the whole point of a bump:
# it is what a local build's startup banner reports.
step "CMakeLists.txt version (local-build source of truth)"
CMAKE_VERSION="$(version_from CMakeLists.txt "$CMAKE_RE")"
if [[ "$CMAKE_VERSION" == "$TARGET" ]]; then
  pass "QILAB_VERSION_* == $TARGET"
else
  fail "CMakeLists.txt declares '$CMAKE_VERSION', target is '$TARGET'"
fi

# version_local.hpp is dead code (nothing includes it) and has been stale since
# 0.4.0. Advisory only -- deleting it is a separate decision from a release.
LOCAL_HPP=src/utils/version_local.hpp
if [[ -f "$LOCAL_HPP" ]]; then
  LOCAL_VERSION="$(version_from "$LOCAL_HPP" "$CPP_RE")"
  if [[ "$LOCAL_VERSION" == "$TARGET" ]]; then
    pass "$LOCAL_HPP == $TARGET"
  else
    warn "$LOCAL_HPP says '$LOCAL_VERSION' -- dead file, nothing includes it (see SKILL.md)"
  fi
fi

# --- release_prepare: changelog extraction ----------------------------------
# Uses the exact awk from .gitlab-ci.yml: index()==1 prefix match, not a regex.
step "release_prepare (CHANGELOG.md section)"
NOTES="$(awk -v v="$TARGET" '
  index($0, "## [" v "]") == 1 { in_section=1 }
  in_section && index($0, "## [") == 1 && index($0, "## [" v "]") != 1 { exit }
  in_section { print }
' CHANGELOG.md)"
if [[ -n "$(printf '%s' "$NOTES" | tr -d '[:space:]')" ]]; then
  pass "extracted $(printf '%s\n' "$NOTES" | wc -l | tr -d ' ') lines of release notes"
  # The heading alone is not enough: release_prepare only checks the file is
  # non-empty, so a heading with no body would publish an empty release.
  BODY="$(printf '%s\n' "$NOTES" | tail -n +2 | tr -d '[:space:]')"
  if [[ -n "$BODY" ]]; then
    pass "the section has a body, not just a heading"
  else
    fail "'## [$TARGET]' has no content under it -- the release notes would be a bare heading"
  fi
else
  fail "CHANGELOG.md has no '## [$TARGET]' section (must be a literal line prefix)"
fi
if grep -Eq '^##[[:space:]]*\[?Unreleased\]?' CHANGELOG.md; then
  warn "an [Unreleased] section still exists; fold it into $TARGET or leave it deliberately empty"
fi

# --- schema stamping --------------------------------------------------------
step "release_prepare (config schema stamping)"
STAMP_OUT="$(mktemp)"
trap 'rm -f "$STAMP_OUT"' EXIT
if python3 scripts/stamp_schema_version.py "$TARGET" "$STAMP_OUT" >/dev/null 2>&1; then
  STAMPED="$(python3 -c "import json;print(json.load(open('$STAMP_OUT')).get('x-qilab-schema-version',''))")"
  if [[ "$STAMPED" == "$TARGET" ]]; then
    pass "stamp_schema_version.py produces x-qilab-schema-version=$TARGET"
  else
    fail "stamped schema carries '$STAMPED', expected '$TARGET'"
  fi
else
  fail "scripts/stamp_schema_version.py failed on docs/config.schema.json (invalid JSON?)"
fi
REPO_STAMP="$(python3 -c \
  "import json;print(json.load(open('docs/config.schema.json')).get('x-qilab-schema-version',''))" 2>/dev/null)"
if [[ "$REPO_STAMP" == "dev" ]]; then
  pass "the in-repo schema is still marked \"dev\" (correct -- CI stamps the packaged copy)"
else
  warn "docs/config.schema.json is stamped '$REPO_STAMP', not \"dev\"; the repo copy should stay unversioned"
fi

# --- vendored schema drift (advisory, cross-repo) ---------------------------
step "qilab_web_control vendored schema (advisory)"
WEB_SCHEMA=../web_control_server/qilab_web_control/app/config/config.schema.json
[[ -f "$WEB_SCHEMA" ]] || WEB_SCHEMA=/home/mrkolev/work/web_control_server/qilab_web_control/app/config/config.schema.json
if [[ -f "$WEB_SCHEMA" ]]; then
  if python3 - "$WEB_SCHEMA" <<'PY'
import json, sys
def load(p):
    d = json.load(open(p))
    d.pop('x-qilab-schema-version', None)   # differs by design: "dev" vs stamped
    return d
sys.exit(0 if load('docs/config.schema.json') == load(sys.argv[1]) else 1)
PY
  then
    pass "vendored copy matches docs/config.schema.json"
  else
    warn "the vendored copy has DRIFTED -- after releasing, re-vendor it into"
    warn "  qilab_web_control and bump its app/config/VENDORED_SCHEMA_VERSION to $TARGET"
  fi
else
  skip "no qilab_web_control checkout found (this check skips in CI too)"
fi

# --- repository state -------------------------------------------------------
step "repository state"
if [[ -z "$(git status --porcelain)" ]]; then
  pass "working tree clean"
else
  warn "uncommitted changes -- the tag must land on a commit with the bumped CMakeLists.txt"
fi
if git rev-parse -q --verify "refs/tags/v$TARGET" >/dev/null; then
  fail "tag v$TARGET already exists locally"
else
  pass "tag v$TARGET is free locally"
fi
if git ls-remote --exit-code --tags origin "refs/tags/v$TARGET" >/dev/null 2>&1; then
  fail "tag v$TARGET already exists on origin -- a release may already be published"
else
  pass "tag v$TARGET is free on origin"
fi
printf '  \033[36mINFO\033[0m  on branch %s; releases have been cut from development\n' \
  "$(git rev-parse --abbrev-ref HEAD)"

# --- build / test / packaging ----------------------------------------------
if [[ $QUICK -eq 1 ]]; then
  step "build / test / release_prepare"
  skip "--quick: skipped (run without --quick before reporting)"
else
  step "build + test jobs (host build, BUILD_TESTING=ON)"
  BUILD_LOG="$(mktemp)"
  if cmake -S . -B build -DCMAKE_C_COMPILER="$(which gcc)" -DCMAKE_CXX_COMPILER="$(which g++)" \
       -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON >"$BUILD_LOG" 2>&1 &&
     cmake --build build -j"$(nproc)" >>"$BUILD_LOG" 2>&1; then
    pass "configure + build"
    if ctest --test-dir build --output-on-failure >>"$BUILD_LOG" 2>&1; then
      pass "ctest: $(grep -E 'tests passed' "$BUILD_LOG" | tail -1)"
    else
      fail "ctest failed:"
      grep -E "tests failed|\(Failed\)" "$BUILD_LOG" | head -10 | sed 's/^/        /'
    fi
  else
    fail "build failed:"
    grep -iE "error" "$BUILD_LOG" | head -5 | sed 's/^/        /'
  fi
  rm -f "$BUILD_LOG"

  step "release_prepare (aarch64 packaging, via scripts/release_local.sh)"
  if ! command -v aarch64-linux-gnu-g++ >/dev/null; then
    warn "aarch64-linux-gnu-g++ not installed -- cannot mirror release_prepare locally"
    warn "  install with: apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
  else
    # release_local.sh starts with `rm -rf build-release`. A build-release left
    # behind by a root/Docker build is undeletable by this user, and the failure
    # surfaces as a wall of "rm: Permission denied" that looks like a script bug.
    UNDELETABLE="$(find build-release ! -user "$(id -un)" -print -quit 2>/dev/null)"
    if [[ -n "$UNDELETABLE" ]]; then
      fail "build-release/ contains files owned by another user (e.g. $UNDELETABLE)"
      fail "  release_local.sh cannot 'rm -rf build-release'. Clear it first:"
      fail "      sudo rm -rf build-release"
      REL_LOG=""
    else
    REL_LOG="$(mktemp)"
    if ./scripts/release_local.sh "v$TARGET" >"$REL_LOG" 2>&1; then
      pass "release_local.sh completed (cross-build + tarballs + release notes)"

      # The generated header is the only reliable way to see what the binary
      # reports: the banner string is assembled at runtime from three ints.
      GEN_HEADER=build-release/generated/qilab/version.hpp
      if [[ -f "$GEN_HEADER" ]]; then
        GEN_VERSION="$(version_from "$GEN_HEADER" "$CPP_RE")"
        if [[ "$GEN_VERSION" == "$TARGET" ]]; then
          pass "the built binary will report $GEN_VERSION"
        else
          fail "the built binary would report '$GEN_VERSION', not '$TARGET'"
        fi
      else
        fail "no generated version header at $GEN_HEADER"
      fi

      # release_publish hard-requires both of these exact paths.
      for artifact in "dist/qilab-v$TARGET-linux-arm64.tar.gz" \
                      "dist-update/qilab-update-v$TARGET.tar.gz"; do
        if [[ -f "$artifact" ]]; then
          pass "release_publish artifact exists: $artifact"
        else
          fail "missing artifact release_publish tests for: $artifact"
        fi
      done

      if [[ -s dist/release_notes.md ]]; then
        pass "dist/release_notes.md is non-empty"
      else
        fail "dist/release_notes.md is empty -- release_publish refuses to publish it"
      fi

      if grep -qE "aarch64|ARM aarch64" build-release/binary_arch.txt 2>/dev/null; then
        pass "binary is aarch64: $(cut -d, -f2 build-release/binary_arch.txt | tr -d ' ')"
      else
        fail "binary is not aarch64 -- release_prepare greps binary_arch.txt for it"
      fi

      # The schema is consumed straight out of the deployed release directory,
      # so verify the copy that actually ships, not just the stamping script.
      TARBALL="dist/qilab-v$TARGET-linux-arm64.tar.gz"
      if [[ -f "$TARBALL" ]]; then
        SHIPPED="$(tar -xzOf "$TARBALL" "qilab-$TARGET/config.schema.json" 2>/dev/null |
          python3 -c "import json,sys;print(json.load(sys.stdin).get('x-qilab-schema-version',''))" 2>/dev/null)"
        if [[ "$SHIPPED" == "$TARGET" ]]; then
          pass "the schema inside the tarball is stamped $TARGET"
        else
          fail "the tarball's schema carries '$SHIPPED', expected '$TARGET'"
        fi
      fi
    else
      fail "release_local.sh failed:"
      tail -8 "$REL_LOG" | sed 's/^/        /'
    fi
    rm -f "$REL_LOG"
    fi
  fi
fi

step "$([[ $FAILURES -eq 0 ]] && echo "READY for v$TARGET" || echo "NOT READY for v$TARGET")"
echo "  $FAILURES blocking, $WARNINGS advisory"
[[ $FAILURES -eq 0 ]]
