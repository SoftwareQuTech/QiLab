# QiLab — Quantum Network Simulation Framework

## Overview

A C++20 framework for quantum network simulation focusing on distributed quantum computing and entanglement management.

![Architecture Overview](docs/images/MultithreadedApplication-BigOverview_1.drawio.png)

## Core Components

### Simulation Engine
- High-performance quantum circuit simulation via Stim integration
- Modular adapter system supporting multiple backends
- Real-time state tracking and measurement

### Entanglement Management
- Dynamic entanglement distribution
- Bell state generation and tracking
- Resource allocation and deallocation

### Message Broker
- Centralized command routing
- Multi-protocol support
- Priority-based message handling

### Transport Layer
- Pluggable transport protocols (Mock/SPI/Network)
- Hardware abstraction interface
- Quality of Service management

## System Architecture

![System Flow](docs/images/MultithreadedApplication-BigOverview.drawio.png)

## Getting Started

### Prerequisites
- C++20 compiler (GCC 10+ or Clang 10+)
- CMake 3.15+
- Linux environment (Windows via WSL2 supported)

### Quick Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Project Structure
```
src/
  ├── Simulator/         # Core simulation engine
  ├── EntanglementMgr/   # Entanglement coordination
  ├── MessageBroker/     # Command routing
  ├── Adapters/          # Backend implementations
  └── Common/            # Shared utilities
```

## Configuration

Settings managed via YAML configuration:
- Simulation parameters
- Node topology
- Transport protocols
- Logging levels

## Development

### Building with Tests
```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build
```

### Documentation
- Architecture diagrams in design
- API documentation via Doxygen
- Integration examples

## License

MIT License - See LICENSE

## Acknowledgments

- [Stim](https://github.com/quantumlib/stim) quantum circuit simulator
- Contributors and maintainers

---

**Note**: Active development project - APIs subject to change