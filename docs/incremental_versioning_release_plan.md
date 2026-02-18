# Incremental Plan: Versioning, Release Management, and Infrastructure Updates

## Goal
Introduce versioning, release flow, and deployment updates in very small, safe steps with clear verification after each change.

Principles:
- Small changes only
- One concern at a time
- Verify each step before continuing
- Prefer simple, explicit logic over abstraction

## Phase Checklist

- [x] Phase 0 — Baseline (no behavior change)
- [x] Phase 1 — Minimal versioning (local hardcoded)
- [x] Print version at startup
- [x] Phase 2 — CMake-managed version (local)
- [ ] Phase 3 — CI-aware versioning
- [x] Phase 4 — Local release management MVP
- [x] Phase 5 — CI/CD release management MVP
- [x] Phase 6 — Infrastructure update procedure (manual-first)
- [x] Phase 7 — Infrastructure automation (optional)

---

## Phase 0 — Baseline (No Behavior Change)

### Scope
- Confirm current local build and run behavior
- Confirm current CI build/test status

### Change Size
- No code changes

### Verification
- Local build command succeeds
- App runs as today
- CI pipeline remains green

### Exit Criteria
- Baseline commands and expected outputs are known and repeatable

### Status (Verified)
- Local build artifact exists: `build/qilab`
- Test discovery works: `ctest -N` reports 85 tests
- CI configuration is present and structured (`build`, `test`, `coverage` stages)
- CI "pipeline green" must be confirmed in GitLab UI (external to local workspace)

---

## Phase 1 — Minimal Versioning (Local Only, Hardcoded)

### Scope
- Add static version constants in one place (major/minor/patch)
- Print version at app startup

### Change Size
- 1–2 files max

### Verification
- Run app locally and see `X.Y.Z` once at startup
- No functional behavior changes

### Exit Criteria
- Version string visible in runtime output

### Status (Verified)
- Added local version constants in one place: `src/utils/version_local.hpp`
- No runtime behavior change yet (startup output unchanged by design in this phase)
- Added startup version print in `application.cpp` using local constants

---

## Phase 2 — CMake-Managed Version (Local)

### Scope
- Move version source of truth to `CMakeLists.txt`
- Generate tiny header from CMake template
- App reads version from generated header

### Change Size
- Small focused edits in CMake + startup include/use

### Verification
- Change version in CMake, rebuild, confirm startup output changes

### Exit Criteria
- Version controlled from CMake only

### Status (Implemented)
- Added CMake version variables in `CMakeLists.txt`
- Added generated header template `src/utils/version.hpp.in`
- App now includes generated header (`qilab/version.hpp`)
- Added git short commit extraction in CMake and exposed it as `VERSION_GIT_COMMIT`
- Startup version log now includes commit suffix

---

## Phase 3 — CI-Aware Versioning

### Scope
- CI tag build (`vX.Y.Z`) uses tag version
- Non-tag CI build uses CMake version + short commit suffix
- Local behavior stays unchanged

### Change Size
- CI config only (plus optional tiny CMake variable wiring)

### Verification
- One branch build and one tag build produce expected version format

### Exit Criteria
- CI version strings are deterministic and traceable

### Status (Implemented)
- `CMakeLists.txt` now accepts `QILAB_CI_TAG` (or `CI_COMMIT_TAG` env) and parses `vX.Y.Z` / `X.Y.Z`
- On valid tag, CMake version fields are overridden from tag values
- GitLab CI configure commands pass `-DQILAB_CI_TAG=${CI_COMMIT_TAG}` in build/test jobs

---

## Phase 4 — Local Release Management MVP

### Scope
- Add minimal local release script
- Build `Release`
- Package tarball with binary + essential config/example files

### Change Size
- 1 script file

### Verification
- Extract tarball on local machine
- Run executable from extracted folder

### Exit Criteria
- Repeatable local release artifact generation

### Status (Implemented)
- Added local script: `scripts/release_local.sh`
- Script does: Release configure/build of `qilab` and packages `dist/qilab-<version>.tar.gz`
- Included files: executable, `README.md`, optional `docs/config_example.yaml`

---

## Phase 5 — CI/CD Release Management MVP

### Scope
- Add one CI release job, tag-triggered only
- Build release binary and upload artifact tarball
- No deployment automation yet

### Change Size
- CI config only

### Verification
- Push test tag and confirm downloadable artifact exists

### Exit Criteria
- CI creates release artifact on tags

### Status (Implemented)
- Added `release_mvp` job in `.gitlab-ci.yml`
- Job runs only on tags (`CI_COMMIT_TAG`)
- Job cross-compiles ARM64 binary (`aarch64`) and publishes `dist/qilab-<version>-linux-arm64.tar.gz` as artifact
- Job verifies architecture using `file` and stores `build-release/binary_arch.txt`
- No deployment or environment update is performed in CI (artifact-only MVP)

---

## Phase 6 — Infrastructure Update Procedure (Manual-First)

### Scope
Define and use a simple manual deployment sequence:
1. Upload tarball to target machine
2. Extract to versioned directory
3. Switch active path/symlink to new version
4. Restart service
5. Run health/version check

Include rollback procedure:
1. Switch active path/symlink back to previous version
2. Restart service
3. Re-check health/version

### Change Size
- Procedure only (documented steps)

### Verification
- Dry run on non-production target

### Exit Criteria
- Safe manual upgrade + rollback process proven

### Manual Runbook (MVP)

Assumptions:
- Service name: `qilab`
- Install root: `/opt/qilab`
- Active symlink: `/opt/qilab/current`
- Versioned releases: `/opt/qilab/releases/<version>`

1) Prepare target directories (once)
```bash
sudo mkdir -p /opt/qilab/releases
```

2) Upload/copy release tarball to target
```bash
scp dist/qilab-0.0.3.tar.gz <user>@<host>:/tmp/
```

3) Extract into versioned directory
```bash
VERSION=0.0.3
sudo mkdir -p /opt/qilab/releases/$VERSION
sudo tar -xzf /tmp/qilab-$VERSION.tar.gz -C /opt/qilab/releases/$VERSION --strip-components=1
sudo chmod +x /opt/qilab/releases/$VERSION/qilab
```

4) Record current active target (for rollback)
```bash
PREV_TARGET=$(readlink -f /opt/qilab/current || true)
echo "$PREV_TARGET"
```

5) Switch active version + restart service
```bash
sudo ln -sfn /opt/qilab/releases/$VERSION /opt/qilab/current
sudo systemctl restart qilab
sudo systemctl status qilab --no-pager
```

6) Health/version check
```bash
journalctl -u qilab -n 50 --no-pager
```
Expected: startup log includes new `QILab vX.Y.Z` (and commit suffix).

7) Rollback (if needed)
```bash
sudo ln -sfn "$PREV_TARGET" /opt/qilab/current
sudo systemctl restart qilab
sudo systemctl status qilab --no-pager
journalctl -u qilab -n 50 --no-pager
```

### Status (Implemented)
- Added concrete manual deployment runbook with copy-paste commands
- Includes explicit rollback steps using previous symlink target
- No automation added (manual-first preserved)

---

## Phase 7 — Infrastructure Automation (Optional, Later)

### Scope
- Add very small install/update helper script for operators
- Script should implement the already-proven manual procedure only

### Change Size
- 1 script file

### Verification
- One controlled upgrade test + one rollback test

### Exit Criteria
- Automated path is no more complex than manual path

### Status (Implemented)
- Added `scripts/update_release.sh`
- Script performs: extract tarball to `/opt/qilab/releases/<version>`, switch `/opt/qilab/current`, restart service
- On restart failure, script auto-rolls back to previous symlink target and retries service restart
- Kept intentionally minimal and aligned to manual runbook logic

---

## Recommended Working Mode
For each phase:
1. Implement only that phase
2. Verify immediately
3. Stop and review
4. Continue only after approval

This keeps risk low and prevents broad, hard-to-debug changes.
