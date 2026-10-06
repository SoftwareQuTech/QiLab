# QiLab — Quantum Network Simulation Framework

QiLab is a C++20 framework for simulating quantum internet networks with a focus on command routing, entanglement coordination, and pluggable data transport. It integrates the [Stim](https://github.com/quantumlib/stim) stabilizer simulator and provides runners for simulation, entanglement, and data transmission.

> **QiLab currently works only with QNodeOS. QNodeOS is not included with QiLab, and QiLab's license grants no rights to QNodeOS.** See the [QNodeOS notice](#qnodeos-notice).

## Demo
![QiLab demo: from Python to qubits and back](docs/demo/qilab_demo.mp4)

A two-node remote state preparation run, then four nodes running entanglement-based key distribution in parallel. For the interactive version, download [docs/demo/qilab_demo.html](docs/demo/qilab_demo.html) and open it in a browser (Space pauses, F toggles fullscreen).

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
- Graceful shutdown: send SIGINT or SIGTERM; each runner's `on_abort()` is called before the threads are joined.

### Telemetry output paths
Both instrument sinks write to fixed filenames under a directory that can be
relocated by env var. This is the interface `qilab_web_control` reads to offer
the files for download — the same static-file hand-off already used for
`config.schema.json`.

| Artifact | Default path | Override (first match wins) |
|----------|--------------|------------------------------|
| DAQ records (JSONL) | `~/.qilab/logs/data_acquisition.jsonl` | `QILAB_DAQ_PATH` (full path), then `QILAB_DAQ_DIR` |
| Profiler durations (CSV) | `~/.qilab/logs/profile_durations.csv` | `QILAB_PROFILER_PATH` (full base), then `QILAB_PROFILER_DIR` |
| Profiler events (CSV) | `~/.qilab/logs/profile_events.csv` | same as above |

CSV columns:
- `profile_durations.csv` — `Event,Max[ns],Min[ns],Count,Avg[ns]`. `Avg[ns]` is
  the exact mean (`total_runtime / count`), so it always falls within
  `[Min, Max]`; a count of 0 reports 0.
- `profile_events.csv` — `Message,TimestampNs` (free-form `PROF_EVENT` markers).

The deployed systemd unit sets `QILAB_DAQ_DIR` and `QILAB_PROFILER_DIR` under
`/var/log/qilab`, because it runs with `ProtectHome=true` and cannot write `$HOME`.
Files are overwritten per run.

Flush behaviour:
- **DAQ** is flushed once per second from the main (non-real-time) thread and
  again after all threads join, so the JSONL is readable *while a run is in
  progress* and complete after shutdown.
- **Profiling** is inherently shutdown-only: each runner merges its thread-local
  counters into the global container as it exits, so the CSVs are written once,
  after every thread has joined. There is no partial-run profiling snapshot.

## Configuration
QiLab loads node/link configuration from `~/.qilab/config/nodes.yaml` by default. A minimal default file is created automatically if none exists. Override the path with either env var (checked in this order):
1. `QILAB_CONFIG_PATH` — an explicit full path, used as-is.
2. `QILAB_CONFIG_DIR` — a directory; `nodes.yaml` is read from inside it.

**[docs/config_guide.md](docs/config_guide.md) is the authoritative reference** for the full format (node types, qubit-array noise models, entanglement links, the `system` section). A short, current example:

```yaml
nodes:
  - name: alice
    node_type: NV_CENTER
    two_qubit_gate_noise_model:
      type: DEPOLARIZING
      depolarizing_probability: 0.02
    qubit_arrays:
      - num_qubits: 2
        single_gate_delay_ns: 200
        qubit_decoherence_noise_model:
          type: BASIC
          t2_value: 1000
        gate_noise_model:
          type: DEPOLARIZING
          depolarizing_probability: 0.001
  - name: bob
    qubit_arrays:
      - num_qubits: 2
        single_gate_delay_ns: 200
        qubit_decoherence_noise_model:
          type: BASIC
          t2_value: 1000
        gate_noise_model:
          type: DEPOLARIZING
          depolarizing_probability: 0.001

entanglement_links:
  - node_a: alice
    node_b: bob
    attempt_model:
      type: BASIC
      attempt_duration_ns: 1000
      batch_size: 100
      p_success: 0.5
      rng_seed: 42
    link_noise_model:
      type: BASIC
      p_depolarizing: 0.01
      p_dephasing: 0.02

system:
  transmission_cycle_time_us: 5
  qubit_noise_enabled: true
  entanglement_noise_enabled: true
```

## Data Acquisition
Structured, low-overhead JSONL recording for research/experiment data — separate from the human-readable log. Compiles away entirely when `ENABLE_DATA_ACQUISITION=OFF`; gated at runtime by `QILAB_DAQ=1` (see [Run](#run) above).

```cpp
#include "logger/data_acquisition_api.hpp"

DAQ_RECORD("entanglement").set("attempt", 42).set("success", true).set("fidelity", 0.91);

// Guard expensive field construction:
if (DAQ_ACTIVE()) {
    DAQ_RECORD("state").set("dump", expensive_to_string());
}

DAQ_FLUSH(); // application.cpp already does this each second and at shutdown
```

Each record auto-prefixes a `ts_us` (epoch microseconds) timestamp and channel name, producing one JSON object per line at the DAQ output path (see [Telemetry output paths](#telemetry-output-paths)):
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

## License
Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner.

QiLab is released under the [Clear BSD License](LICENSE) (SPDX: `BSD-3-Clause-Clear`). This license grants no patent rights. Every source file carries the matching SPDX header.

### QNodeOS notice
1. **Compatibility.** QiLab is currently compatible only with QNodeOS. It has no other supported use.
2. **Not included.** QNodeOS is separate software. It is not part of QiLab and is not provided, distributed or sublicensed with QiLab.
3. **No rights granted.** The QiLab license covers QiLab only. It grants no rights of any kind in QNodeOS, express or implied, including copyright, patent and trademark rights.
4. **Separate terms.** You must get QNodeOS from its rights holders. Any use of QNodeOS is governed solely by its own license terms.
5. **Name only.** The name "QNodeOS" is used here only to describe compatibility. It does not imply endorsement by the QNodeOS rights holders.

### Third-party software
Third-party dependencies are fetched at build time and remain under their own licenses:
- [Stim](https://github.com/quantumlib/stim) (v1.15.0): Apache License 2.0. Statically linked into the `qilab` binary. Release archives include its license as `LICENSE.stim`.
- [GoogleTest](https://github.com/google/googletest) (tests only, not shipped): BSD 3-Clause License.

---
Active development project — APIs and wiring may change.
