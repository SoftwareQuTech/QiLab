# QILab Configuration Guide

> **Maintainers:** if you change the config format (add/remove a noise model, field, or node type), update [`config.schema.json`](config.schema.json) in this directory in the same change. It is the source of truth for the `qilab_web_control` structured config editor.
>
> The release pipeline packages `config.schema.json` into the release tarball, stamped with that release's version (`x-qilab-schema-version`), so it lands next to the binary at `/opt/qilab/current/config.schema.json` on every deploy via `update_release.sh`'s symlink swap. `qilab_web_control` should point `QILAB_WEB_SCHEMA_PATH` there to always track the exact schema of the qilab version it's controlling.

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
entanglement_links:          # optional
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

system:                      # optional
  transmission_cycle_time_us: 5
  qubit_noise_enabled: true
  entanglement_noise_enabled: true

nodes:                       # required, must be non-empty
  - ...
```

| Key | Required | Description |
|-----|----------|-------------|
| `nodes` | Yes | List of quantum node definitions |
| `entanglement_links` | No | Per-link entanglement models, one entry per physical link between two configured nodes (see [Entanglement Links](#entanglement-links)) |
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
| `quantum_operations_delay_ns` | No | map | Node-wide operation durations. See [Operation Delays](#operation-delays) |
| `two_qubit_gate_noise_model` | No | map | Two-qubit gate noise for this node; omitted means noiseless. See [Gate Noise Models](#gate-noise-models) |

Field order within a node does not matter.

### Operation Delays

`quantum_operations_delay_ns` sets how long each operation takes, in nanoseconds.
All keys are optional and default to `0`. The node's response for an operation is
withheld until its delay has elapsed, and the qubit's decoherence clock is
advanced accordingly.

| Key | Type | Description |
|-----|------|-------------|
| `two_qubit_gate_rotation` | integer ns | Two-qubit gate duration |
| `measurement` | integer ns | Measurement duration |
| `entanglement` | integer ns | Entanglement operation duration |
| `qubit_move` | integer ns | Qubit move duration |
| `qubit_swap` | integer ns | Qubit swap duration |
| `qubit_init` | integer ns | Qubit initialisation duration |

```yaml
quantum_operations_delay_ns:
  two_qubit_gate_rotation: 300
  measurement: 400
  entanglement: 500
  qubit_move: 600
  qubit_swap: 400
  qubit_init: 200
```

> **Note:** the **single-qubit** gate duration is deliberately *not* here. It is a
> qubit array property (`single_gate_delay_ns`), because arrays on one node
> differ in it — e.g. communication vs. memory qubits. There is no node-level
> fallback: a `single_gate_rotation` key here is rejected as unknown.

### Node Types

| Value | Description |
|-------|-------------|
| `GENERIC` | Default generic quantum node |
| `NV_CENTER` | Nitrogen-vacancy center |
| `ION_TRAP` | Ion trap hardware |
| `PROCESSOR` | Quantum processor |

---

## Qubit Arrays

A node must have at least one qubit array. An array is a group of qubits sharing a
decoherence model, a gate noise model, and a single-qubit gate duration. Arrays
exist precisely so that one node can mix qubit kinds — for example a fast, noisy
communication qubit alongside slower, longer-lived memory qubits.

```yaml
qubit_arrays:
  # Communication qubit: fast gates, shorter T2.
  - num_qubits: 1
    single_gate_delay_ns: 200
    qubit_decoherence_noise_model:
      type: PHASE_FLIP
      t2_value: 900
    gate_noise_model:
      type: DEPOLARIZING
      depolarizing_probability: 0.001
  # Memory qubits: slower gates, longer T2, different gate fidelity.
  - num_qubits: 4
    single_gate_delay_ns: 1200
    qubit_decoherence_noise_model:
      type: BASIC
      t2_value: 2000
    gate_noise_model:
      type: DEPOLARIZING
      depolarizing_probability: 0.005
```

### Qubit Array Fields

| Key | Required | Type | Description |
|-----|----------|------|-------------|
| `num_qubits` | Yes | positive integer | Number of qubits in this array |
| `single_gate_delay_ns` | Yes | integer ns | Single-qubit gate duration for this array's qubits |
| `qubit_decoherence_noise_model` | Yes | map | Idle (in-memory) decoherence model. See [Qubit Decoherence Noise Models](#qubit-decoherence-noise-models) |
| `gate_noise_model` | Yes | map | Single-qubit gate noise model. See [Gate Noise Models](#gate-noise-models) |

`num_qubits` may be written inline after `-`:
```yaml
  - num_qubits: 4
    single_gate_delay_ns: 200
    qubit_decoherence_noise_model: ...
    gate_noise_model: ...
```

Field order within a qubit array does not matter.

The node's total `num_qubits` is the sum across all arrays. Each array's models
and delay apply to **only that array's qubits** — arrays never share or override
one another.

---

## Qubit Decoherence Noise Models

Set under `qubit_decoherence_noise_model` inside a qubit array. This is the
**idle, in-memory** noise a qubit accumulates between operations — distinct from
the per-gate noise configured by `gate_noise_model`. All models require `type`.

### `BASIC`
Simple T2 dephasing model.

```yaml
qubit_decoherence_noise_model:
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
qubit_decoherence_noise_model:
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

### `PHASE_FLIP`
Phase flip (dephasing) channel.

```yaml
qubit_decoherence_noise_model:
  type: PHASE_FLIP
  t2_value: 1000
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `t2_value` | Yes | float ≥ 0 | T2 dephasing time in nanoseconds |

---

## Gate Noise Models

Gate noise is a depolarizing channel applied **per gate instruction**, separate
from the idle decoherence of
[`qubit_decoherence_noise_model`](#qubit-decoherence-noise-models). The two are
composed into a single Pauli channel before being applied.

Granularity follows the hardware:

| Applies to | Configured at | Key | Maximum `p` |
|------------|---------------|-----|-------------|
| Single-qubit gates (and pre-measurement basis rotations) | each **qubit array** | `gate_noise_model` | `0.75` |
| Two-qubit gates | the **node** | `two_qubit_gate_noise_model` | `0.9375` |

### `DEPOLARIZING`

Set under `gate_noise_model` inside a qubit array.

```yaml
gate_noise_model:
  type: DEPOLARIZING
  depolarizing_probability: 0.001
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `depolarizing_probability` | Yes | float in [0, 0.75] | `p` for the single-qubit depolarizing channel |

The channel is `E(rho) = (1-p) rho + p/3 (X rho X + Y rho Y + Z rho Z)`, which
relates to gate fidelity by `F = 1 - 2p/3`.

### Two-qubit gate noise

The two-qubit channel is `p/15` uniform over the 15 non-identity two-qubit Pauli
pairs, with `F = 1 - 4p/5`. It is **correlated** across the pair, so it is applied
via the backend's own two-qubit depolarizing primitive rather than as two
independent single-qubit channels.

```yaml
nodes:
  - name: alice
    two_qubit_gate_noise_model:
      type: DEPOLARIZING
      depolarizing_probability: 0.02
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `depolarizing_probability` | Yes | float in [0, 0.9375] | `p` for the two-qubit depolarizing channel |

At `p = 0` the two-qubit channel is skipped entirely.

> **Rejected, not repaired:** a probability above its channel's maximal mixing
> point (`0.75` single-qubit, `0.9375` two-qubit) is refused at config-load time
> with a parse error — the config does not load. Nothing is clamped, and no
> silently-noiseless model is installed.

> **Not covered:** `QUBIT_MOVE` and `QUBIT_SWAP` apply idle decoherence only —
> they carry no gate depolarizing noise today.

---

## Entanglement Links

Set under `entanglement_links` at the top level — a list, one entry per physical link between two configured nodes. The whole section is optional; a link with no entry configured for it simply has no entanglement noise applied.

Each link is identified by a pair of node names (`node_a`/`node_b`), which **must match** the `name` of a node under `nodes` — an unknown name, `node_a == node_b`, or a duplicate link entry is rejected at load time.

Each link carries two independently-typed models:
- `attempt_model` — batching/delay/success-probability, consumed by the entanglement manager.
- `link_noise_model` — the Pauli noise channel applied to the qubit pair on a successful attempt, consumed by the simulator manager.

```yaml
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
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `node_a` | Yes | string | Name of the first node of the link |
| `node_b` | Yes | string | Name of the second node of the link |
| `attempt_model` | Yes | map | Entanglement attempt model. See [Entanglement Attempt Models](#entanglement-attempt-models) |
| `link_noise_model` | Yes | map | Entanglement channel model. See [Entanglement Channel Models](#entanglement-link-models) |

### Entanglement Attempt Models

Set under `attempt_model` inside a link.

#### `BASIC`

```yaml
attempt_model:
  type: BASIC
  attempt_duration_ns: 1000
  batch_size: 100
  p_success: 0.5
  rng_seed: 42
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `attempt_duration_ns` | Yes | integer > 0 | Duration of one entanglement attempt, in nanoseconds |
| `batch_size` | Yes | integer > 0 | Maximum attempts per batch before reporting failure |
| `p_success` | Yes | float in [0, 1] | Per-attempt success probability |
| `rng_seed` | Yes | integer ≥ 0 | Seed for the batch-sampling RNG |

### Entanglement Link Noise Models

Set under `link_noise_model` inside a link.

#### `BASIC`

```yaml
link_noise_model:
  type: BASIC
  p_depolarizing: 0.01
  p_dephasing: 0.02
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `p_depolarizing` | Yes | float in [0, 1] | Depolarizing probability applied to the pair on success |
| `p_dephasing` | Yes | float in [0, 1] | Dephasing probability applied to the pair on success |

---

## System Configuration

Set under `system` at the top level. The whole section is optional; each key defaults independently if omitted.

```yaml
system:
  transmission_cycle_time_us: 5
  qubit_noise_enabled: true
  entanglement_noise_enabled: true
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `transmission_cycle_time_us` | No | float ≥ 0 | Period of the transmission runner's busy-spin cycle, in microseconds. Defaults to `5`. |
| `qubit_noise_enabled` | No | boolean (`true`/`false`) | Enables applying qubit-in-memory (dephasing) noise during simulation. Defaults to `true`. |
| `entanglement_noise_enabled` | No | boolean (`true`/`false`) | Enables applying entanglement-link noise during simulation. Defaults to `true`. |

---

## Full Example

```yaml
# QILab Node Configuration

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

nodes:
  - name: alice
    node_type: NV_CENTER
    two_qubit_gate_noise_model:
      type: DEPOLARIZING
      depolarizing_probability: 0.02
    quantum_operations_delay_ns:
      two_qubit_gate_rotation: 300
      measurement: 400
      entanglement: 500
    qubit_arrays:
      - num_qubits: 2
        single_gate_delay_ns: 1200
        qubit_decoherence_noise_model:
          type: ADVANCED
          t1_value: 5000
          t2_value: 2000
          gate_error_rate: 0.001
        gate_noise_model:
          type: DEPOLARIZING
          depolarizing_probability: 0.005
      - num_qubits: 1
        single_gate_delay_ns: 200
        qubit_decoherence_noise_model:
          type: PHASE_FLIP
          t2_value: 900
        gate_noise_model:
          type: DEPOLARIZING
          depolarizing_probability: 0.001

  - name: bob
    node_type: ION_TRAP
    two_qubit_gate_noise_model:
      type: DEPOLARIZING
      depolarizing_probability: 0.03
    qubit_arrays:
      - num_qubits: 4
        single_gate_delay_ns: 250
        qubit_decoherence_noise_model:
          type: BASIC
          t2_value: 800
        gate_noise_model:
          type: DEPOLARIZING
          depolarizing_probability: 0.002

  - name: relay
    node_type: GENERIC
    qubit_arrays:
      - num_qubits: 2
        single_gate_delay_ns: 200
        qubit_decoherence_noise_model:
          type: PHASE_FLIP
          t2_value: 900
        gate_noise_model:
          type: DEPOLARIZING
          depolarizing_probability: 0.001
      - num_qubits: 2
        single_gate_delay_ns: 900
        qubit_decoherence_noise_model:
          type: BASIC
          t2_value: 800
        gate_noise_model:
          type: DEPOLARIZING
          depolarizing_probability: 0.004
```

In this example:
- `alice` has 3 total qubits (2 + 1) across two arrays that differ in decoherence
  model, gate fidelity, and gate duration.
- `bob` has 4 qubits with a single basic array.
- `relay` has 4 qubits split across a phase-flip array and a basic array, and
  omits `two_qubit_gate_noise_model`, so its two-qubit gates are noiseless.
- The link between `alice` and `bob` has its own attempt and link noise models;
  `relay` has no configured link, so no entanglement noise is applied to it.
