# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Placeholder for upcoming changes.

## [0.3.0] - 2026-03-25

### Added
- Noise modeling harness with configurable, per-model noise definitions, including example modeling configurations and models, and primitives for per-qubit noise.
- Configuration parsing for minimal noise models: parsers follow each model's configuration and fail on unknown models or parameters.
- Configurable operation durations: every operation can have a fixed, configurable delay that is decoupled from the main simulator execution loop. Verified with entanglement and single-node applications.
- Error/check for attempting to entangle a qubit with itself.
- JSON schema validation file for the new web configuration.

### Changed
- Moved simulator registration and execution into a dedicated manager; the runner now only communicates and cycles.
- Added a configuration options header to centralize and simplify configuration management (start of a broader configuration overhaul).
- Unified folder naming to CamelCase.
- Busy-wait on the transmission core; updated the transmission runner.

### Fixed
- Update script: fixed the simulator symlink handling and other update-script issues.
