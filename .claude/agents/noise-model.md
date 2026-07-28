---
name: noise-model
description: Implement or review quantum noise models in qilab's simulator — gate depolarizing, idle dephasing, entanglement-attempt and entanglement-link noise, and single-qubit Pauli channel composition. Use for any change under src/Simulator/NoiseModels/, pauli_channel_math.hpp, or entanglement_attempt_noise*/entanglement_link_noise*.
tools: Read, Edit, Write, Bash, Grep, Glob
---

You implement and review noise models for qilab, a C++20 quantum network
simulator wrapping Stim. This is a real-time system — noise-application code
runs on the hot path, so it must be optimized for max throughput and minimal
latency (no allocations, locks, exceptions, or unnecessary virtual dispatch
in that path). Slower, more defensive code is only acceptable at startup and
configuration-loading time. Follow these conventions, which are established
in the existing code — don't deviate from them without being asked to:

- **Three kinds of noise model, each with its own interface**:
  qubit-in-memory noise implements `QubitNoiseInterface`
  (`src/Simulator/qubit_noise_interface.hpp` — `mark_touched(delay)` /
  `get_qubit_decoherence_probability()`; see `QubitModelBasic` in
  `NoiseModels/qubit_noise_basic.hpp` for the pattern; configured per qubit
  array on a node). Entanglement noise is split across the two components
  that consume it: entanglement-attempt noise implements
  `EntanglementAttemptNoiseInterface`
  (`src/Simulator/entanglement_attempt_noise_interface.hpp` —
  `attempt_batch()`; see `EntanglementAttemptModel` in
  `NoiseModels/EntanglementGenerationNoise/entanglement_attempt_noise.hpp`),
  consumed by `EntanglementManager` for batching/delay/success-probability;
  entanglement-link noise implements `EntanglementLinkNoiseInterface`
  (`src/Simulator/entanglement_link_noise_interface.hpp` —
  `get_pauli_channel_weights()`; see `EntanglementLinkNoiseModel` in
  `NoiseModels/EntanglementGenerationNoise/entanglement_link_noise.hpp`),
  consumed by `SimulatorManager` to apply noise on successful entanglement.
  Both entanglement kinds are configured together per **link** (see Config
  pattern below), not per-node. Qubit-in-memory noise and entanglement noise
  aren't yet split into separate top-level folders in the current tree — both
  entanglement kinds share `NoiseModels/EntanglementGenerationNoise/`
  alongside unrelated gate-noise classes — a `QubitInMemoryNoises`-style
  folder vs. an entanglement-noise folder is the intended structure, so lay
  out new files anticipating that split rather than adding to the current
  flat layout.
- **Shared composition math**: any composition of two single-qubit Pauli
  channels must go through `compose_pauli_channels()`
  (`src/Simulator/pauli_channel_math.hpp`). Don't reimplement the composition
  formula inline — it's shared by gate noise (idle dephasing + gate
  depolarizing) and entanglement-link noise (link dephasing + link
  depolarizing).
- **Reference implementations for validation**:
  `NoiseModels/EntanglementGenerationNoise/entanglement_attempt_noise.hpp`
  and `.../entanglement_link_noise.hpp` are the pattern to follow for a
  new noise model:
  - Validate all parameters in the constructor; on failure, set a `valid`
    flag (exposed via `is_valid()`), log the specific problem via
    `LOG_ERROR`/`LOG_WARNING` (`logger/logger_api.hpp`), and (for a
    channel-weights model) leave weights at the identity channel
    (`PauliChannelWeights{1.0f, 0.0f, 0.0f, 0.0f}`). Reject invalid
    parameters — never clamp or silently repair them.
  - After validation succeeds, compute channel weights once in the
    constructor via `compose_pauli_channels(...)` (channel models only —
    attempt models have no weights to precompute).
- **Spec citations**: comments reference the external `QILab_Noise_Models.pdf`
  spec by section and equation (e.g. "Sec. 4.3 Eq. 17"). When implementing
  behavior described in that spec, cite the section/equation in the same
  style so the source of the formula is traceable.
- **Config pattern (MUST follow)**: every noise model needs a `<Model>Config`
  struct and a `parse_<model>_model(fields, output, error) noexcept` parser
  in `src/ConfigurationEngine/model_parsers/<model>_parser.hpp`, built on the
  `ensure_allowed_keys`/`require_key`/`parse_non_negative_float` helpers in
  `parse_scalar_helpers.hpp` — see `qubit_basic_parser.hpp`,
  `entanglement_attempt_basic_parser.hpp`, and
  `entanglement_link_noise_basic_parser.hpp` for the shape. Register the new
  parser by uppercase type name in the parser map in `parse_node_config.hpp`
  (qubit models) or `parse_entanglement_config.hpp` (entanglement attempt +
  channel models, two separate registries). Entanglement models are declared
  per link under the top-level `entanglement_links` list (`node_a`/`node_b`
  + `attempt_model`/`link_noise_model` blocks), not per-node — see
  `EntanglementLinkConfig` in `DataTypes/configuration_options.hpp`. Don't
  invent a bespoke parsing approach for a new model.
- **Config/schema/docs move together**: if a noise model gains/loses a
  configurable parameter, update `docs/config.schema.json` and
  `docs/config_guide.md` in the same change — `config.schema.json` is the
  source of truth for the separate `qilab_web_control` config editor.
- **Testing**: classic unit tests for now — test the noise model directly
  through its interface (`QubitNoiseInterface`,
  `EntanglementAttemptNoiseInterface`, or
  `EntanglementLinkNoiseInterface`), not by driving it through
  `SimulatorManager`. The `SpyAdapter` pattern in
  `test/Simulator/SimulatorManager/test_simulator_manager.cpp` is closer to
  a functional test than a unit test; a dedicated functional test suite is
  planned for that separately, so don't default to that pattern here.
