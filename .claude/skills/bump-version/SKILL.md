---
name: bump-version
description: Prepare a qilab release for a given version — bump the CMake version, close the CHANGELOG section, and verify every CI gate a vX.Y.Z tag has to clear before it is pushed. Use whenever the user names a version to release ("bump to 0.4.3", "release 0.5.0", "update the version").
---

# Releasing a version of qilab

The user names a target version. Get the repository into a state where pushing
`v<version>` publishes a release, and **prove it** by running the gates locally
rather than discovering a failure after the tag exists.

Tags are hard to retract once pushed — a GitLab release may already be created.
So: prepare, verify, report, and **only push the tag after the user explicitly
says to**.

This mirrors the `bump-version` skill in `qilab_web_control`; the two projects
release independently but deliberately follow the same shape.

## 1. Establish the target

Version is `X.Y.Z`, no `v` prefix in files, `v` prefix on the tag only.

If the user did not say which version, ask; do not infer a bump level from the
changes. If they name a version that is already tagged, stop and say so.

Do not trust `git tag --sort=-v:refname | head -1` to find the previous
release: `v0.99.1` and `v0.99.2` are old test tags and sort above everything
real. Use `CHANGELOG.md` for the release history — as of v0.4.2 it also has a
`## [0.4.1]` section that was never tagged.

## 2. Edit what a release needs

**`CMakeLists.txt`** — `QILAB_VERSION_MAJOR` / `MINOR` / `PATCH` near the top.
This is the version a *local* build reports through
`src/utils/version.hpp.in` -> `qilab/version.hpp` -> the startup banner in
`application.cpp`.

CI overrides these from the tag (`-DQILAB_CI_TAG=${CI_COMMIT_TAG}`), so the
released binary is correct even if you forget — which is exactly why this
drifts and why it is worth being deliberate about. `git show
v0.4.2:CMakeLists.txt` still reports `0.4.0`: it has never been bumped since
v0.4.0, so every local build since has misreported its version.

**`CHANGELOG.md`** — the heading must be a literal `## [<version>]` at the start
of a line. `release_prepare` extracts the section with `awk` using
`index($0, "## [" v "]") == 1`, a **prefix match, not a regex**, so:

```markdown
## [0.4.3]
```

A trailing ` - <date>` is also matched (`## [0.4.0] - 2026-07-24` in the
history), but recent entries omit it. Follow whatever the neighbouring sections
do. Then:

- If an `## [Unreleased]` section holds this release's notes, rename it to the
  version heading. Do not leave the content duplicated under both.
- Otherwise write the section from the actual commits since the last release
  (`git log --oneline <last-tag>..HEAD`), grouped `### Fixed` / `### Added` /
  `### Changed` / `### CI`, matching the voice of the existing entries: what
  broke or changed, and *why it mattered* — these entries become the published
  release notes verbatim.
- The section must not be empty. `release_prepare` hard-fails on an empty
  extraction.

Leave alone:

- `CHANGELOG.md` history — older sections are the record.
- **`docs/config.schema.json`'s `"x-qilab-schema-version": "dev"`.** The repo
  copy stays `"dev"` on purpose; `scripts/stamp_schema_version.py` overwrites it
  with the real version when packaging, so the schema in the tarball is stamped
  and the working copy stays honest about being unreleased.
- **Version literals in `docs/` and `scripts/`.** The `0.0.3` in
  `update_release.sh`, `release_local.sh` and `docs/systemd_service_setup.md`
  are generic usage placeholders, not the project version. Unlike
  qilab_web_control, this repo has no docs that cite the current release.
- **`src/utils/version_local.hpp`.** Nothing includes it — the real header is
  generated from `version.hpp.in`. It is stale dead code (`0.4.0`). Mention it
  to the user rather than quietly editing it; deleting it is the right fix, but
  that is a separate decision, not part of a release.

## 3. Verify

```bash
.claude/skills/bump-version/check_release.sh <version>
```

It mirrors the gates in `.gitlab-ci.yml` and names the job each check stands in
for: tag shape, the CMake version, `release_prepare`'s changelog extraction,
`build`, `test`, and the full `release_prepare` packaging run (via
`scripts/release_local.sh`, which is an exact copy of that job's script) —
including that the aarch64 binary really is aarch64, that both tarballs exist,
and that the schema inside the tarball is stamped with the target version.

The packaging check cross-compiles stim from scratch (`release_local.sh` wipes
`build-release` first), so a full run takes many minutes. Use `--quick` to run
only the cheap file-state checks while iterating on the changelog, then one full
run before reporting.

The host build/test check reuses the existing `build/` directory with the
`Debug` flags from `CLAUDE.md`, not CI's `Release` + `--coverage`. That keeps it
incremental instead of forcing a full rebuild on every run, but it is not
bit-identical to the `build`/`test` jobs; a failure that appears only under
`Release` will not be caught here.

Blocking failures must be fixed. Advisory results with known meanings:

- **`version_local.hpp` disagrees** — expected; see above.
- **Vendored schema drift in qilab_web_control** — the check compares
  `docs/config.schema.json` against that project's
  `app/config/config.schema.json` and skips when the checkout is absent. If it
  reports drift and this release changes the schema, qilab_web_control needs to
  re-vendor it *after* this release and bump its
  `app/config/VENDORED_SCHEMA_VERSION` to the new stamp. The schema ships with
  the binary and deploys atomically with it (see `CLAUDE.md`), so the ordering
  is: release qilab, then re-vendor into the web control.
- **Uncommitted changes** — fine mid-preparation, but the tag has to land on a
  commit that already contains the bumped `CMakeLists.txt` and the closed
  changelog section.

If the host build/test check fails, confirm the failures are actually a release
blocker and not unrelated work in progress: `git status` first. A dirty
`src/` means the tests are reporting on the working tree, not on the commit you
are about to tag.

## 4. Report, then wait

Tell the user what changed, the check results, and the exact commands to
release:

```bash
git commit -am "Release <version>"
git push
git tag v<version>
git push origin v<version>
```

Push the tag only on an explicit go-ahead. If asked to do the whole thing, still
run the checks first and stop if any gate fails.

Note the branch: releases have been cut from `development`. Confirm with the
user which branch the tag should land on if it is not obvious.

## Notes that catch people out

- **A malformed tag fails silently in the worst possible way.** CMake accepts
  `vX.Y.Z` or `X.Y.Z`; anything else fails the regex and CMake falls back to the
  hardcoded `QILAB_VERSION_*` — no warning, no error, just a released binary
  whose banner reports the wrong version. This has already happened here: `v0.1`
  and `v0.2` are both in the tag list.
- Unlike qilab_web_control there is **no `workflow:` rule**, so *any* tag shape
  starts a full pipeline, and `release_prepare`/`release_publish` run on `if:
  $CI_COMMIT_TAG` — any tag at all. A malformed tag therefore builds, tests,
  and only then fails in `release_prepare` on the missing changelog section.
  You get a red pipeline instead of silence, but late and misleadingly.
- The artifact filenames embed the **tag** (`qilab-v0.4.3-linux-arm64.tar.gz`)
  while the directory inside uses the **stripped version**
  (`qilab-0.4.3/qilab-0.4.3-linux-arm64`). That asymmetry is why
  `release_publish` tests for `qilab-${CI_COMMIT_TAG}-...`. Do not "fix" it.
- `release_prepare`'s artifacts `expire_in: 1 week`, and `release_publish`
  publishes links to them. The release keeps working, but those asset links
  stop resolving after a week — expected, not a bug to chase.
- The binary builds the version string at runtime from three ints, so there is
  no version literal to grep for in the ELF. To confirm what a build will
  report, read `build-release/generated/qilab/version.hpp`.
- A `build-release/` left behind by a root/Docker build cannot be deleted by
  your user, and `release_local.sh` opens with `rm -rf build-release`. The
  failure looks like a broken script; it is stale root-owned state. The check
  detects it up front and tells you to `sudo rm -rf build-release`.
- `scripts/update_release.sh` ships inside the *update* tarball, so each release
  replaces the update script on devices that update to it. A fix to the update
  mechanism cannot reach a device through the mechanism it fixes: that first hop
  needs a manual install.
