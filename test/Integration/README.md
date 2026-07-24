# Integration tests

Scope: real transmission-processor hardware I/O — actual SPI-originated
commands arriving from external devices, exercised through the real
`ITransmissionHandler` implementation (not `MockTransmissionHandler` or
`SpyAdapter`).

This is **not** the place for adapter/protocol-building tests that don't
need real hardware — those belong in `test/Functional/`, which already
uses the spy/mocked adapter for that purpose (see
`test/Functional/Simulator/SimulatorManager/test_simulator_manager.cpp`).

Currently empty: no hardware is available yet to exercise this against.
Enable the CMake option below once it is, and populate
`CMakeLists.txt` in this directory following the existing Unit/Functional
pattern (one `gtest_discover_tests(... PROPERTIES LABELS "integration")`
target).

```
cmake -S . -B build ... -DQILAB_BUILD_INTEGRATION_TESTS=ON
```

Not wired into CI — `.gitlab-ci.yml` never sets this option, so this
directory stays fully inert until someone deliberately opts in locally or
a future CI job is added.
