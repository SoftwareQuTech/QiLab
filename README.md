# QiLab — Quantum Network Simulation Framework

QiLab is a C++20 framework for simulating quantum internet networks with a focus on command routing, entanglement coordination, and pluggable data transport. It integrates the [Stim](https://github.com/quantumlib/stim) stabilizer simulator and provides runners for simulation, entanglement, and data transmission.

![Architecture Overview](docs/images/MultithreadedApplication-BigOverview_1.drawio.png)

## System Flow
![System Flow](docs/images/MultithreadedApplication-BigOverview.drawio.png)

## At a Glance
- **ConfigurationEngine**: loads and validates the YAML node/link configuration (`ConfigurationEngine`)
- **DataTypes**: wire-format command/response structs shared across the pipeline (`DataTypes`)
- **Simulation**: Stim-backed adapters behind a pluggable interface, driven by a manager that owns all per-node state (`Simulator/SimulatorAdapters`, `Simulator/SimulatorManager`)
- **Noise models**: per-qubit decoherence and entanglement-generation noise, configurable per node/link (`Simulator/NoiseModels`)
- **Entanglement**: request matching, batched success/failure, and delayed execution (`Simulator/EntanglementManager`, `Simulator/SimulatorRunners/entanglement_runner`)
- **Messaging**: lock-free broker routing commands and responses between runner threads (`MessageBroker`)
- **Transport**: pluggable handlers for SPI/mock comms (`DataTransmission`)
- **Logging & Instrumentation**: a real-time-safe logger, low-overhead profiler, and structured data-acquisition sink (`logger`)

## Project Layout
```
src/
  ConfigurationEngine/         # YAML config loading, parsing, model parsers
  DataTransmission/            # Transmission handlers, processor, runner
  DataTypes/                   # Wire command/response structs
  logger/                      # StandardLogger, Profiler, DataAcquisition
  MessageBroker/                # Thread-safe broker and ring buffer
  Simulator/
    SimulatorAdapters/          # ISimulator implementations (Stim-backed, mock)
    SimulatorManager/            # Per-node dispatch, timing, noise application
    EntanglementManager/          # Request matching, batching, delay
    NoiseModels/                   # Qubit decoherence + entanglement-generation noise
    SimulatorRunners/               # Threads driving simulator/entanglement flows
  utils/                        # Thread utilities and version info
application.cpp                 # Entry point wiring runners together
docs/                            # Config guide, schema, architecture diagrams, release docs
scripts/                          # Log-analysis and release helper scripts
test/
  Unit/                          # Single class/function, all collaborators mocked/stubbed
  Functional/                     # Multiple real components wired together
  Integration/                     # Scaffold for real-hardware tests (see test/Integration/README.md)
CHANGELOG.md                       # Release history
```

## Build
Prerequisites:
- CMake 3.14+ (project minimum)
- C++20 toolchain. By default CMake cross-compiles for `aarch64-linux-gnu`. Override for host builds and enable tests in one step:
  ```bash
  cmake -S . -B build -DCMAKE_C_COMPILER=$(which gcc) -DCMAKE_CXX_COMPILER=$(which g++) -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
  cmake --build build -j$(nproc)
  ```
  `CMAKE_BUILD_TYPE` defaults to `Release` if unset. `Debug` additionally forces `ENABLE_LOGGING=ON`.

Build options:
| Option | Default | Effect |
|---|---|---|
| `ENABLE_PROFILING` | ON | Compiles in the low-overhead TLS profiler |
| `ENABLE_LOGGING` | ON | Compiles in `StandardLogger`; always on regardless of build type (Debug additionally *forces* it on if you tried to turn it off) |
| `ENABLE_DATA_ACQUISITION` | ON | Compiles in the structured JSONL data-acquisition sink (see below) |
| `BUILD_TESTING` | ON (via `include(CTest)`) | Builds the `test/` targets for `ctest` |

Artifacts appear in `build/`: the `qilab` executable, static libraries, and (if `BUILD_TESTING=ON`) test binaries under `build/test/`.

## Run
```bash
./build/qilab
```

Runtime notes:
- `application.cpp` wires up and starts all three runner threads (simulator, entanglement, transmission) unconditionally.
- Thread pinning is controlled via the `SIMULATOR_CORE`, `ENTANGLEMENT_CORE`, and `TRANSMISSION_CORE` constants in `application.cpp`.
- `QILAB_LOG_LEVEL` (env var) sets the log level (`DEBUG`/`ERROR`/`OFF`); defaults to `DEBUG` if unset.
- `QILAB_DAQ=1` (env var) enables the data-acquisition sink (see below); anything else or unset leaves it disabled.
- Profiling data is dumped to CSV (`test_trace_events.csv`, `test_trace_durations.csv`) when profiling is enabled.
- Graceful shutdown: send SIGINT or SIGTERM; each runner's `on_abort()` is called before the threads are joined.

## Configuration
QiLab loads node/link configuration from `~/.qilab/config/nodes.yaml` by default. A minimal default file is created automatically if none exists. Override the path with either env var (checked in this order):
1. `QILAB_CONFIG_PATH` — an explicit full path, used as-is.
2. `QILAB_CONFIG_DIR` — a directory; `nodes.yaml` is read from inside it.

**[docs/config_guide.md](docs/config_guide.md) is the authoritative reference** for the full format (node types, qubit-array noise models, entanglement links, the `system` section). A short, current example:

```yaml
nodes:
  - name: alice
    node_type: NV_CENTER
    qubit_arrays:
      - num_qubits: 2
        noise_model:
          type: BASIC
          t2_value: 1000
  - name: bob
    qubit_arrays:
      - num_qubits: 2
        noise_model:
          type: BASIC
          t2_value: 1000

entanglement_links:
  - node_a: alice
    node_b: bob
    attempt_model:
      type: BASIC
      attempt_duration_ns: 1000
      batch_size: 100
      p_success: 0.5
      rng_seed: 42
    channel_model:
      type: BASIC
      p_depolarizing: 0.01
      p_dephasing: 0.02

system:
  transmission_cycle_time_us: 5
  qubit_noise_enabled: true
  entanglement_noise_enabled: true
```

> `docs/config_example.yaml` predates the current schema (it still references
> removed config keys) — use `docs/config_guide.md`'s worked example instead
> until that file is refreshed.

## Data Acquisition
Structured, low-overhead JSONL recording for research/experiment data — separate from the human-readable log. Compiles away entirely when `ENABLE_DATA_ACQUISITION=OFF`; gated at runtime by `QILAB_DAQ=1` (see [Run](#run) above).

```cpp
#include "logger/data_acquisition_api.hpp"

DAQ_RECORD("entanglement").set("attempt", 42).set("success", true).set("fidelity", 0.91);

// Guard expensive field construction:
if (DAQ_ACTIVE()) {
    DAQ_RECORD("state").set("dump", expensive_to_string());
}

DAQ_FLUSH(); // call periodically from a non-real-time thread, or at shutdown
```

Each record auto-prefixes a `ts_us` (epoch microseconds) timestamp and channel name, producing one JSON object per line at `~/.qilab/logs/data_acquisition.jsonl`:
```json
{"ts_us":1751371200123456,"channel":"entanglement","attempt":42,"success":true,"fidelity":0.910000}
```
Analyze with, e.g., `pandas.read_json(path, lines=True)`.

This is distinct from `scripts/analyze_gate_noise_log.py` and `scripts/extract_measurements.py`, which parse the **plain-text `StandardLogger` log** (not the DAQ JSONL) for per-qubit measurement statistics, noise-probability/fidelity checks, and timing analysis — see each script's `--help` for options.

## Testing
`test/` is split by scope: `Unit/` (one class/function, all collaborators mocked/stubbed), `Functional/` (multiple real components wired together, doubling only the outermost hardware-adapter boundary), and a scaffolded `Integration/` (real transmission-processor hardware, gated behind `-DQILAB_BUILD_INTEGRATION_TESTS=ON`, pending hardware availability — see `test/Integration/README.md`).

```bash
cmake -S . -B build -DCMAKE_C_COMPILER=$(which gcc) -DCMAKE_CXX_COMPILER=$(which g++) -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Every test is labeled `unit` or `functional` (`ctest -L unit` / `ctest -L functional` to filter). See `CLAUDE.md`'s Testing convention for the reference pattern to follow when adding new tests.

## Deployment
For running QiLab as a systemd service in production (Raspberry Pi 5 target), see [docs/systemd_service_setup.md](docs/systemd_service_setup.md). For the versioning/release-flow rollout plan, see [docs/incremental_versioning_release_plan.md](docs/incremental_versioning_release_plan.md).

## Diagrams & Documentation
- Architecture diagrams: `docs/Design/*.drawio`
- Configuration reference: [docs/config_guide.md](docs/config_guide.md)
- Release history: [CHANGELOG.md](CHANGELOG.md)
- Core interfaces live under `src/`; headers include Doxygen comments for runners, brokers, transmission handlers, and profiling APIs.

---
Active development project — APIs and wiring may change.
