# QiLab

C++20 framework simulating quantum internet networks: command routing,
entanglement coordination, and pluggable data transport, built on top of the
[Stim](https://github.com/quantumlib/stim) stabilizer simulator. See
[README.md](README.md) for the full architecture overview, run instructions,
and node-configuration format — this file covers what the README doesn't.

## Build & test
```bash
cmake -S . -B build -DCMAKE_C_COMPILER=$(which gcc) -DCMAKE_CXX_COMPILER=$(which g++) -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```
CMake defaults to cross-compiling for `aarch64-linux-gnu` — always pass the
explicit `CMAKE_C_COMPILER`/`CMAKE_CXX_COMPILER` overrides above for host
builds. `Debug` forces `ENABLE_LOGGING=ON`; `ENABLE_PROFILING` defaults ON.

## Subsystems (`src/`)
`ConfigurationEngine`, `DataTransmission`, `DataTypes`, `logger`,
`MessageBroker`, `Simulator` (+ `SimulatorAdapters`, `SimulatorManager`,
`NoiseModels`), `SimulatorRunners`, `utils` — see README's "At a Glance" for
what each does.

## Conventions
- **Real-time system: hot path is performance-critical.** Anything on the
  per-operation/per-gate/per-qubit path (simulator adapter calls, noise
  application, entanglement attempts) must be optimized for max throughput
  and minimal latency — avoid allocations, locks, exceptions, and
  unnecessary virtual dispatch there. Slower, less-optimized code is only
  acceptable at startup and configuration-loading time.
- **Noise models have three kinds, each with its own config pattern (MUST
  follow)**: qubit-in-memory noise (`QubitNoiseInterface`,
  `src/Simulator/qubit_noise_interface.hpp` — `mark_touched()` /
  `get_qubit_decoherence_probability()`, e.g. `QubitModelBasic` in
  `NoiseModels/qubit_noise_basic.hpp`, configured per qubit array on a node);
  entanglement-attempt noise (`EntanglementAttemptNoiseInterface`,
  `src/Simulator/entanglement_attempt_noise_interface.hpp` —
  `attempt_batch()`, e.g. `EntanglementAttemptModel` in
  `NoiseModels/EntanglementGenerationNoise/entanglement_attempt_noise.hpp`,
  consumed by `EntanglementManager` for batching/delay/success-probability);
  and entanglement-channel noise (`EntanglementChannelNoiseInterface`,
  `src/Simulator/entanglement_channel_noise_interface.hpp` —
  `get_pauli_channel_weights()`, e.g. `EntanglementChannelModel` in
  `NoiseModels/EntanglementGenerationNoise/entanglement_channel_noise.hpp`,
  consumed by `SimulatorManager` to apply noise on successful entanglement).
  The two entanglement kinds are configured together per **link** (a pair of
  node names, `entanglement_links` at the config's top level — not per-node,
  not global), each entry carrying its own `attempt_model` and
  `channel_model` blocks; see `docs/config_guide.md`. Qubit-in-memory vs.
  entanglement noise models are not yet in separate top-level folders in the
  current tree (both entanglement kinds share
  `NoiseModels/EntanglementGenerationNoise/`, alongside unrelated gate-noise
  classes) — that split is planned and new noise work should anticipate it.
  - Config pattern (see `src/ConfigurationEngine/model_parsers/`): each
    model has its own `<Model>Config` struct, a `parse_<model>_model(fields,
    output, error) noexcept` function in `model_parsers/<model>_parser.hpp`
    using the `ensure_allowed_keys`/`require_key`/`parse_non_negative_float`
    helpers (`parse_scalar_helpers.hpp`), registered by uppercase type name
    in the parser map in `parse_node_config.hpp` (qubit noise) /
    `parse_entanglement_config.hpp` (entanglement attempt + channel noise,
    two separate registries). New noise models must follow this pattern, not
    a bespoke one.
- **Config/schema stay in sync**: any config format change (new noise model,
  field, or node type) must update `docs/config.schema.json` in the same
  change — it's the source of truth for the separate `qilab_web_control`
  config editor. See `docs/config_guide.md`. The schema is packaged into the
  release tarball and version-stamped by CI (`.gitlab-ci.yml`
  `release_prepare`); it's not a separately versioned artifact — it always
  ships and deploys atomically with the qilab binary it matches.
- **Noise models reject invalid parameters, they don't repair them**:
  constructors validate and set an `is_valid()` flag, logging via
  `LOG_ERROR`/`LOG_WARNING` (`logger/logger_api.hpp`) rather than clamping;
  invalid models fall back to the identity channel. Composition of Pauli
  channels goes through `compose_pauli_channels()`
  (`src/Simulator/pauli_channel_math.hpp`), shared by gate noise and
  entanglement-channel noise — don't reimplement it inline. Comments cite
  the external `QILab_Noise_Models.pdf` spec by section/equation (e.g. "Sec.
  4.3 Eq. 17"); keep that citation style when touching this code.
- **Testing**: `test/` is split into `Unit/`, `Functional/`, and
  `Integration/` (scaffold only, gated behind `QILAB_BUILD_INTEGRATION_TESTS`,
  requires real hardware not yet available). Unit tests exercise a single
  class/function with all collaborators mocked/stubbed (`test/Unit/`, built
  into the `unit_tests` binary, plus the compile-flag-gated
  `prof_tests_enabled/disabled` and `daq_tests_enabled/disabled` binaries).
  Functional tests wire multiple real, collaborating components together
  (e.g. `SimulatorManager` + real noise models), doubling only the
  outermost hardware-adapter boundary via `SpyAdapter` — see
  `test/Functional/Simulator/SimulatorManager/test_simulator_manager.cpp`
  and `test/Functional/Entanglement/test_entanglement_manager.cpp` as the
  reference pattern for new functional tests (built into the
  `functional_tests` binary). Integration tests will cover real
  transmission-processor hardware I/O (actual SPI-originated commands from
  external devices) once hardware is available; until then, functional
  tests use the spy/mocked adapter for building commands and protocols
  instead.
- Folders are named CamelCase.
- `ISimulator` (`src/Simulator/sim_interface.hpp`) is the dispatch boundary
  to the actual backend (Stim). Only add a method to it when the operation
  genuinely needs to invoke something on the adapter — don't grow it for
  logic that can live in `SimulatorManager` or elsewhere.
