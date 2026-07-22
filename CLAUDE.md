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
- **Noise models have two kinds, each with its own config pattern (MUST
  follow)**: qubit-in-memory noise (`QubitNoiseInterface`,
  `src/Simulator/qubit_noise_interface.hpp` — `mark_touched()` /
  `get_qubit_decoherence_probability()`, e.g. `QubitModelBasic` in
  `NoiseModels/qubit_noise_basic.hpp`) and entanglement-related noise
  (`EntanglementGenerationNoiseInterface`). These are not yet in separate
  folders in the current tree — that split (a `QubitInMemoryNoises`-style
  folder vs. entanglement noise folder) is planned and new noise work should
  anticipate it.
  - Config pattern (see `src/ConfigurationEngine/model_parsers/`): each
    model has its own `<Model>Config` struct, a `parse_<model>_model(fields,
    output, error) noexcept` function in `model_parsers/<model>_parser.hpp`
    using the `ensure_allowed_keys`/`require_key`/`parse_non_negative_float`
    helpers (`parse_scalar_helpers.hpp`), registered by uppercase type name
    in the parser map in `parse_node_config.hpp` /
    `parse_entanglement_config.hpp`. New noise models must follow this
    pattern, not a bespoke one.
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
  entanglement-generation noise — don't reimplement it inline. Comments cite
  the external `QILab_Noise_Models.pdf` spec by section/equation (e.g. "Sec.
  4.3 Eq. 17"); keep that citation style when touching this code.
- **Testing**: classic unit tests for now. The `SpyAdapter`-based tests in
  `test/Simulator/SimulatorManager/test_simulator_manager.cpp` are closer to
  functional tests than unit tests — a dedicated functional test suite is
  planned for that later, so don't treat that pattern as the general unit
  testing convention to replicate.
- Folders are named CamelCase.
- `ISimulator` (`src/Simulator/sim_interface.hpp`) is the dispatch boundary
  to the actual backend (Stim). Only add a method to it when the operation
  genuinely needs to invoke something on the adapter — don't grow it for
  logic that can live in `SimulatorManager` or elsewhere.
