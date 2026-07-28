# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.4.1]

### Added
- Configurable gate depolarizing noise (`QILab_Noise_Models.pdf` Sec. 2). Each
  qubit array carries a `gate_noise_model` block (`type: DEPOLARIZING`,
  `depolarizing_probability`) driving single-qubit gates and pre-measurement
  basis rotations, replacing the hardcoded `0.3f` placeholder in
  `SimulatorManager`. Each node carries a `two_qubit_gate_noise_model` block for
  two-qubit gates, which previously applied no gate noise at all. Both blocks go
  through the standard registry-based model parser pattern, with the accepted
  maximum taken from the model class's own `MAX_DEPOLARIZING_PROBABILITY` (0.75
  single-qubit, 0.9375 two-qubit) so parser, model, and schema cannot drift.
- `ISimulator::two_qubit_depolarizing_noise()`, backed by Stim's `DEPOLARIZE2`
  in both Stim adapters. The two-qubit depolarizing channel is correlated
  across the pair, so it is not expressible through `single_qubit_noise` or
  `compose_pauli_channels()` (a single-qubit composer) and needs its own
  adapter entry point.
- Per-qubit-array `single_gate_delay_ns`, resolved on the gate hot path from a
  global-qubit-indexed table (mirroring `simulation_qubits`) so the lookup is a
  single indexed load with no search or branch.
- `QILAB_DAQ_PATH` / `QILAB_DAQ_DIR` for relocating data-acquisition output,
  matching the existing `QILAB_PROFILER_PATH` / `QILAB_PROFILER_DIR`
  precedence. Without these, DAQ could not write anything in the deployed
  service, whose systemd unit sets `ProtectHome=true`.
- The generated default config now ships a commented-out second node and
  `entanglement_links` block, so the link format is documented in-file.
  Explanatory prose uses `##` and commented-out config a single `#`, making
  "uncomment the block" an unambiguous per-line edit.

### Changed
- **Breaking config change:** `single_gate_rotation` is removed from the
  node-level `quantum_operations_delay_ns` block and replaced by the required
  per-qubit-array `single_gate_delay_ns`. There is no node-level fallback: gate
  duration differs between arrays on one node (e.g. communication vs. memory
  qubits), so a single source of truth avoids a precedence rule. Configs still
  using the old key are rejected with an unknown-key error.
- **Breaking config change:** a qubit array's `noise_model` is renamed
  `qubit_decoherence_noise_model` — it configures idle in-memory decoherence
  specifically, and now sits next to the unrelated `gate_noise_model`.
- **Breaking config change:** an entanglement link's `channel_model` is renamed
  `link_noise_model`, with the matching C++ rename
  (`EntanglementChannelNoiseInterface` → `EntanglementLinkNoiseInterface`,
  `EntanglementChannelModel` → `EntanglementLinkNoiseModel`,
  `entanglement_channel_noise*` → `entanglement_link_noise*`) so config and code
  share one vocabulary. `get_pauli_channel_weights()` keeps its name — there
  "channel" is the physics term, not the config concept.
- `gate_noise_model` and `single_gate_delay_ns` are both required on every qubit
  array, alongside `num_qubits` and `qubit_decoherence_noise_model`.
- `system.qubit_noise_enabled` now gates gate depolarizing noise (single- and
  two-qubit) in addition to in-memory decoherence: disabling it turns off all
  per-qubit noise. Schema and guide descriptions updated to match.
- DAQ is now flushed once per second from the main thread and again after all
  threads join, so the JSONL is readable while a run is in progress and complete
  at shutdown. Previously `DAQ_FLUSH()` was never called anywhere and records
  reached disk only via static-destructor ordering at exit.
- Profiler output is named `profile_durations.csv` / `profile_events.csv` instead
  of `test_trace_*`, and is dumped only after every thread has joined.
- `profile_durations.csv` gains an `Avg[ns]` column. `EventData` now accumulates
  `total_runtime` and the mean is derived at dump time, rather than carrying a
  running `event_duration_average` — sums merge exactly across threads, whereas
  averaging two means weights unequal sample counts the same. The previous field
  was in fact never written by `stop()` at all, so it was always 0.
- The deployed systemd unit sets `QILAB_DAQ_DIR=/var/log/qilab/daq` and
  `QILAB_DAQ=1`; DAQ was never enabled in the service before.
- `docs/config_example.yaml` and `docs/config_test.yaml` rewritten against the
  current format — both still referenced a removed top-level
  `entanglement_noise_model` key and a nonexistent `DEPOLARIZING` qubit noise
  model. `docs/config_guide.md` gains the previously undocumented
  `quantum_operations_delay_ns` and new gate-noise sections.

### Fixed
- `SimulatorManager::register_node()` filled `simulation_qubits` using the
  node's total qubit count for *every* array instead of each array's own size,
  so on a node with more than one array the last array's noise model silently
  overwrote every qubit in the node. Arrays now cover only their own qubit
  range, and registration is validate-then-commit: an empty array list, a
  non-positive array size, a mismatch between the array sizes and the node's
  `num_qubits`, or the unimplemented `ADVANCED` model are all rejected before
  any manager state is mutated (previously `ADVANCED` returned mid-loop after
  earlier arrays had already been written).
- `application.cpp` ignored `register_node()`'s return value, pushing `-1`/`-2`
  failure codes into the node-id list where they wrapped to 65535/65534 and
  silently mis-keyed entanglement links.
- `EntanglementRunner::on_run()` never called `PROF_THREAD_FLUSH()`, unlike the
  other two runners, so anything that thread profiled would be discarded rather
  than merged into the dumped CSV. (It registers no profiler events today, so
  nothing was lost yet — but any event added there would have been.)
- `PROF_SAVE_DATA()` was called once before the run loop, overwriting the
  previous run's CSVs with empty ones before any thread had merged its data.
- `single_qubit_gate_handler` stopped the two-qubit-gate profiler event on its
  failure path.
- `abort_sim` is now `volatile std::sig_atomic_t` rather than a plain `bool`,
  since it is written from a signal handler.
- `test/Unit/Simulator/NoiseModels/test_gate_noise_models.cpp` included
  `depolarizing_gate_noise.hpp` at its pre-move path, breaking the `unit_tests`
  build.

## [0.4.0] - 2026-07-24

### Added
- Link-based entanglement configuration: `entanglement_links` at the config's
  top level, one entry per physical link (`node_a`/`node_b`, validated
  against configured node names, one entry per unordered pair — a reversed
  duplicate is rejected as a duplicate link, not treated as a second link),
  each carrying an `attempt_model` and a `link_noise_model`.
  `entanglement_noise_enabled` system flag, mirroring `qubit_noise_enabled`.
- `EntanglementAttemptNoiseInterface` (`attempt_batch()`), consumed by
  `EntanglementManager` for batched success-probability/delay, replacing its
  previous hardcoded fixed-delay placeholder.
- `EntanglementLinkNoiseInterface` (`get_pauli_channel_weights()`),
  consumed by `SimulatorManager` to apply a Pauli noise channel — composed
  via the shared `compose_pauli_channels()`
  (`src/Simulator/pauli_channel_math.hpp`) — to the initiator's qubit on
  successful entanglement.
- `system` configuration section (`transmission_cycle_time_us`,
  `qubit_noise_enabled`, `entanglement_noise_enabled`) and per-node
  transmission delay configuration.
- Support for a 0-degree rotation angle: it is a no-op on the quantum state
  instead of producing an error (the no-op guarantee lives solely in the
  adapters — `stim_adapter`/`stim_nv_adapter` — as the single source of
  truth, so every call site, including the one-shot pre-measurement gate
  application, gets the same behavior).
- Structured research data acquisition module (`src/logger/data_acquisition*`,
  opt-in via `QILAB_DAQ=1`, output as JSONL) plus log-parsing/measurement
  scripts (`scripts/analyze_gate_noise_log.py`, `scripts/extract_measurements.py`).
- qilab's config schema (`docs/config.schema.json`) is now packaged into the
  release tarball and stamped with the release version
  (`x-qilab-schema-version`), so it deploys atomically alongside the binary at
  `/opt/qilab/current/config.schema.json` for `qilab_web_control` to consume.
- Test infrastructure split into `test/Unit/`, `test/Functional/`, and a
  scaffolded `test/Integration/` (gated behind `QILAB_BUILD_INTEGRATION_TESTS`,
  pending real transmission-processor hardware), using scope-based criteria:
  unit tests mock/stub every collaborator; functional tests wire multiple
  real, collaborating components together, doubling only the outermost
  hardware-adapter boundary. Consolidated from 14 flat, per-subsystem CTest
  binaries into `unit_tests`/`functional_tests` (plus the compile-flag-gated
  `prof_tests_enabled/disabled`, `daq_tests_enabled/disabled`, and the new
  `logger_tests`), each labeled (`ctest -L unit`/`-L functional`) for future
  CI cadence changes.
- Unit test coverage raised from 76.4%/89.5% (line/function) to 80.7%/94.5%,
  adding direct coverage for previously untested production code: both Stim
  adapters (`stim_adapter.cpp`, `stim_nv_adapter.cpp`), the CRC8/bit-packing
  command helpers, all three thread-loop runners (`SimulatorRunner`,
  `EntanglementRunner`, `DataTransmissionRunner`), the real `StandardLogger`,
  `MockAdapter`, `MockHandler`, `pauli_channel_math`'s composition/clamp
  branches, every `ConfigurationEngine` model parser in isolation, and
  `commands.hpp`'s bit-decode logic (including the one field packed across
  two bytes, `PreMeasureGatesSelection::angle2`). `DataAcquisition` hardened
  with concurrency/stress, auto-spill-on-threshold, reconfigure-while-pending,
  file-open-failure, and numeric/string edge-case tests.
- Full-pipeline functional tests: one test per command type
  (`test/Functional/Pipeline/`), driving a raw command through the real
  `MessageBroker`/runner stack end to end and asserting on the resulting
  response, including success, invalid-qubit, and invalid-node-id paths.
  Entanglement-link configuration is now tested end to end from a real YAML
  config through `EntanglementManager`/`SimulatorManager` to a verified
  applied channel-noise effect
  (`test/Functional/ConfigurationEngine/test_entanglement_config_integration.cpp`).
  Statistical/probabilistic validation
  (`test/Functional/Simulator/test_noise_statistical_validation.cpp`) runs
  many trials of real entanglement generation through the Stim-backed
  adapter and checks empirical Bell-pair correlation-break rates against the
  closed-form rate implied by a loaded (or noiseless) channel-noise model.

### Changed
- Split the previous combined `EntanglementGenerationNoiseInterface` into the
  two interfaces above, one per consuming component, each with its own
  `BASIC` model and parser.
- A failed entanglement attempt batch now fails fast (immediate
  `ENTANGLEMENT_FAILURE` to both sides, no retry) instead of the manager's
  old always-succeeds behavior.
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
- The old, unused global `entanglement_noise_model` config path
  (`EntanglementNoiseInterface`, `EntanglementNoiseBasicModel`) — it was
  parsed but never consumed at runtime.
- `DEPOLARIZING` and `AMPLITUDE_DAMPING` config noise models and their parsers
  (`qubit_depolarizing_parser.hpp`, `qubit_amplitude_damping_parser.hpp`,
  `entanglement_depolarizing_parser.hpp`), superseded by the combined
  depolarizing+dephasing entanglement-generation model above. Supported qubit
  noise models are now `BASIC`, `ADVANCED`, and `PHASE_FLIP`.
- Orphaned, unwired test scaffolding (`test/sample_test.cpp`,
  `test/mocks/mock_logger.cpp`, a stale `test/mocks/mock_transmission.hpp`
  superseded by a consolidated `test/mocks/mock_transmission_handler.hpp`).

### Fixed
- `stim_nv_adapter`: reordered constructor member initialization to avoid a
  segfault.
- Idle dephasing (`T2`) noise model corrections, verified against
  `get_qubit_decoherence_probability()`.
- `TransmissionProcessor::deserialize_command`'s opcode bounds check compared
  against `QUBIT_SWAP` instead of the actual highest implemented opcode,
  `ENTANGLEMENT_SWAP` — so `ENTANGLEMENT_SWAP` frames were silently rejected
  as decode failures before ever reaching the fully-implemented
  `entanglement_swap_handler`.
- `MockAdapter` was missing the `single_qubit_noise`/`allocate_qubits`
  overrides `ISimulator` requires, and had a stale `attempt_entanglement`
  signature (missing `local_qubit`), leaving it unable to be instantiated as
  a concrete adapter.

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
