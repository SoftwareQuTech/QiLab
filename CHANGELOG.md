# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Link-based entanglement configuration: `entanglement_links` at the config's
  top level, one entry per physical link (`node_a`/`node_b`, validated
  against configured node names), each carrying an `attempt_model` and a
  `channel_model`. `entanglement_noise_enabled` system flag, mirroring
  `qubit_noise_enabled`.
- `EntanglementAttemptNoiseInterface` (`attempt_batch()`), consumed by
  `EntanglementManager` for batched success-probability/delay, replacing its
  previous hardcoded fixed-delay placeholder.
- `EntanglementChannelNoiseInterface` (`get_pauli_channel_weights()`),
  consumed by `SimulatorManager` to apply a Pauli noise channel to the
  initiator's qubit on successful entanglement.

### Changed
- Split the previous combined `EntanglementGenerationNoiseInterface` into the
  two interfaces above, one per consuming component, each with its own
  `BASIC` model and parser.
- A failed entanglement attempt batch now fails fast (immediate
  `ENTANGLEMENT_FAILURE` to both sides, no retry) instead of the manager's
  old always-succeeds behavior.

### Removed
- The old, unused global `entanglement_noise_model` config path
  (`EntanglementNoiseInterface`, `EntanglementNoiseBasicModel`) — it was
  parsed but never consumed at runtime.

## [0.4.0] - 2026-07-21

### Added
- Combined entanglement-generation noise model: depolarizing + dephasing composed
  via a shared `compose_pauli_channels()` (`src/Simulator/pauli_channel_math.hpp`),
  together with geometric entanglement success/failure and delay.
- `system` configuration section (`transmission_cycle_time_us`,
  `qubit_noise_enabled`) and per-node transmission delay configuration.
- Support for a 0-degree rotation angle: it is a no-op on the quantum state
  instead of producing an error.
- Structured research data acquisition module (`src/logger/data_acquisition*`,
  opt-in via `QILAB_DAQ=1`, output as JSONL) plus log-parsing/measurement
  scripts (`scripts/analyze_gate_noise_log.py`, `scripts/extract_measurements.py`).
- qilab's config schema (`docs/config.schema.json`) is now packaged into the
  release tarball and stamped with the release version
  (`x-qilab-schema-version`), so it deploys atomically alongside the binary at
  `/opt/qilab/current/config.schema.json` for `qilab_web_control` to consume.

### Changed
- Reorganized noise models into `QubitDecoherenceNoise/` and
  `EntanglementGenerationNoise/` folders (the split anticipated in `CLAUDE.md`),
  replacing the previous flat, per-model files.
- `SimulatorManager` now owns pre-measurement gate selection and entanglement
  measurement; adapters (`stim_adapter`, `stim_nv_adapter`) are stateless with
  respect to per-node basis rotation, fixing a bug where one node's rotation
  could apply to a different node's qubit under interleaved commands.
- Qubit allocation now happens as part of node registration in
  `SimulatorManager`.

### Removed
- `DEPOLARIZING` and `AMPLITUDE_DAMPING` config noise models and their parsers
  (`qubit_depolarizing_parser.hpp`, `qubit_amplitude_damping_parser.hpp`,
  `entanglement_depolarizing_parser.hpp`), superseded by the combined
  depolarizing+dephasing entanglement-generation model above. Supported qubit
  noise models are now `BASIC`, `ADVANCED`, and `PHASE_FLIP`.

### Fixed
- `stim_nv_adapter`: reordered constructor member initialization to avoid a
  segfault.
- Idle dephasing (`T2`) noise model corrections, verified against
  `get_qubit_decoherence_probability()`.

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
