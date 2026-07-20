# QILab Configuration Guide

> **Maintainers:** if you change the config format (add/remove a noise model, field, or node type), update [`config.schema.json`](config.schema.json) in this directory in the same change. It is the source of truth for the `qilab_web_control` structured config editor.

## File Location

QILab resolves the config file path using the following environment variable precedence:

| Priority | Variable | Resolved path |
|----------|----------|---------------|
| 1 (highest) | `QILAB_CONFIG_PATH` | Used as-is |
| 2 | `QILAB_CONFIG_DIR` | `$QILAB_CONFIG_DIR/nodes.yaml` |
| 3 (default) | — | `~/.qilab/config/nodes.yaml` |

If the file does not exist at the resolved path, QILab creates a minimal default config automatically.

---

## File Format

The config file uses a YAML-like block format. Indentation is **2 spaces** per level. Tabs are not supported. Lines beginning with `#` are comments and are ignored.

---

## Top-Level Structure

```yaml
entanglement_noise_model:   # optional
  type: BASIC
  t2_value: 1000

system:                      # optional
  transmission_cycle_time_us: 5
  qubit_noise_enabled: true

nodes:                       # required, must be non-empty
  - ...
```

| Key | Required | Description |
|-----|----------|-------------|
| `nodes` | Yes | List of quantum node definitions |
| `entanglement_noise_model` | No | Noise model for entanglement operations (defaults to `BASIC` with `t2_value: 1000`) |
| `system` | No | QiLab System Configuration — global runtime parameters (see [System Configuration](#system-configuration)) |

Field order at the top level does not matter.

---

## Node Definition

Each entry under `nodes` is a list item. The first field may be written inline after `-` or on its own indented line — both are equivalent:

```yaml
# inline first field (preferred)
nodes:
  - name: alice
    node_type: NV_CENTER
    qubit_arrays:
      ...

# or fully indented
nodes:
  -
    name: alice
    node_type: NV_CENTER
    qubit_arrays:
      ...
```

### Node Fields

| Key | Required | Type | Description |
|-----|----------|------|-------------|
| `qubit_arrays` | Yes | list | One or more qubit arrays (see below). The node's total qubit count is the sum of all array sizes. |
| `name` | No | string | Human-readable label for the node |
| `node_type` | No | enum | Hardware type. See [Node Types](#node-types) |

Field order within a node does not matter.

### Node Types

| Value | Description |
|-------|-------------|
| `GENERIC` | Default generic quantum node |
| `NV_CENTER` | Nitrogen-vacancy center |
| `ION_TRAP` | Ion trap hardware |
| `PROCESSOR` | Quantum processor |

---

## Qubit Arrays

A node must have at least one qubit array. Each array defines a group of qubits with a shared noise model.

```yaml
qubit_arrays:
  - num_qubits: 4
    noise_model:
      type: BASIC
      t2_value: 2000
  - num_qubits: 2
    noise_model:
      type: DEPOLARIZING
      error_rate: 0.01
      gate_time_ns: 50
```

### Qubit Array Fields

| Key | Required | Type | Description |
|-----|----------|------|-------------|
| `num_qubits` | Yes | positive integer | Number of qubits in this array |
| `noise_model` | Yes | map | Noise model for this array. See [Qubit Noise Models](#qubit-noise-models) |

`num_qubits` may be written inline after `-`:
```yaml
  - num_qubits: 4
    noise_model: ...
```

Field order within a qubit array does not matter.

> **Note:** The node-level qubit noise model (used for backward compatibility) is taken from the **first** array's noise model. The node's total `num_qubits` is the sum across all arrays.

---

## Qubit Noise Models

Set under `noise_model` inside a qubit array. All models require `type`.

### `BASIC`
Simple T2 dephasing model.

```yaml
noise_model:
  type: BASIC
  t2_value: 1000
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `t2_value` | Yes | float ≥ 0 | T2 dephasing time in nanoseconds |

---

### `ADVANCED`
Full T1/T2 relaxation with gate error.

```yaml
noise_model:
  type: ADVANCED
  t1_value: 5000
  t2_value: 2000
  gate_error_rate: 0.001
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `t1_value` | Yes | float ≥ 0 | T1 relaxation time in nanoseconds |
| `t2_value` | Yes | float ≥ 0 | T2 dephasing time in nanoseconds |
| `gate_error_rate` | Yes | float ≥ 0 | Per-gate error probability |

---

### `DEPOLARIZING`
Symmetric depolarizing channel.

```yaml
noise_model:
  type: DEPOLARIZING
  error_rate: 0.01
  gate_time_ns: 100
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `error_rate` | Yes | float ≥ 0 | Depolarizing error probability per gate |
| `gate_time_ns` | Yes | float ≥ 0 | Gate duration in nanoseconds |

---

### `AMPLITUDE_DAMPING`
Amplitude damping (energy relaxation) channel.

```yaml
noise_model:
  type: AMPLITUDE_DAMPING
  t1_ns: 3000
  t2_ns: 1500
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `t1_ns` | Yes | float ≥ 0 | T1 relaxation time in nanoseconds |
| `t2_ns` | Yes | float ≥ 0 | T2 dephasing time in nanoseconds |

---

### `PHASE_FLIP`
Phase flip (dephasing) channel.

```yaml
noise_model:
  type: PHASE_FLIP
  t2_value: 1000
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `t2_value` | Yes | float ≥ 0 | T2 dephasing time in nanoseconds |

---

## Entanglement Noise Models

Set under `entanglement_noise_model` at the top level. Defaults to `BASIC` with `t2_value: 1000` if omitted.

### `BASIC`

```yaml
entanglement_noise_model:
  type: BASIC
  t2_value: 1000
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `t2_value` | Yes | float ≥ 0 | T2 dephasing time in nanoseconds |

---

### `DEPOLARIZING`

```yaml
entanglement_noise_model:
  type: DEPOLARIZING
  error_rate: 0.05
  gate_time_ns: 200
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `error_rate` | Yes | float ≥ 0 | Depolarizing error probability |
| `gate_time_ns` | Yes | float ≥ 0 | Gate duration in nanoseconds |

---

## System Configuration

Set under `system` at the top level. The whole section is optional; each key defaults independently if omitted.

```yaml
system:
  transmission_cycle_time_us: 5
  qubit_noise_enabled: true
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `transmission_cycle_time_us` | No | float ≥ 0 | Period of the transmission runner's busy-spin cycle, in microseconds. Defaults to `5`. |
| `qubit_noise_enabled` | No | boolean (`true`/`false`) | Enables applying qubit-in-memory (dephasing) noise during simulation. Defaults to `true`. |

---

## Full Example

```yaml
# QILab Node Configuration

entanglement_noise_model:
  type: DEPOLARIZING
  error_rate: 0.05
  gate_time_ns: 200

system:
  transmission_cycle_time_us: 5
  qubit_noise_enabled: true

nodes:
  - name: alice
    node_type: NV_CENTER
    qubit_arrays:
      - num_qubits: 2
        noise_model:
          type: ADVANCED
          t1_value: 5000
          t2_value: 2000
          gate_error_rate: 0.001
      - num_qubits: 1
        noise_model:
          type: AMPLITUDE_DAMPING
          t1_ns: 3000
          t2_ns: 1500

  - name: bob
    node_type: ION_TRAP
    qubit_arrays:
      - num_qubits: 4
        noise_model:
          type: DEPOLARIZING
          error_rate: 0.01
          gate_time_ns: 50

  - name: relay
    node_type: GENERIC
    qubit_arrays:
      - num_qubits: 2
        noise_model:
          type: PHASE_FLIP
          t2_value: 900
      - num_qubits: 2
        noise_model:
          type: BASIC
          t2_value: 800
```

In this example:
- `alice` has 3 total qubits (2 + 1) across two arrays with different noise models.
- `bob` has 4 qubits with a single depolarizing array.
- `relay` has 4 qubits split across a phase-flip array and a basic array.
