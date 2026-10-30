# Building the FRiKKTUNE VST3

This repository contains the plugin source; it does not contain a verified VST3 binary.

## Requirements
- CMake 3.22+
- C++17 compiler/toolchain
- JUCE 8.0.6 (CMake fetches it from GitHub when network access is available)
- Platform audio SDK/toolchain requirements for JUCE

## Build
From this directory:

```bash
cmake -S . -B build-vst3 -DFRIKKTUNE_BUILD_PLUGIN=ON -DFRIKKTUNE_BUILD_DETECTOR_TESTS=ON -DFRIKKTUNE_BUILD_SHIFTER_TESTS=ON
cmake --build build-vst3 --config Release --parallel
ctest --test-dir build-vst3 --output-on-failure
```

On Linux, the VST3 target may require additional system development packages required by JUCE. Install only the packages appropriate for your distribution and toolchain.

The generated VST3 bundle is typically found under `build-vst3/FRiKKTUNE_artefacts/Release/VST3/`. Verify the actual path after building.

## Offline environment
If GitHub cannot be reached, CMake cannot fetch JUCE. Obtain JUCE 8.0.6 from the official JUCE source through a network-enabled machine, then configure CMake to use a local JUCE source tree. The build must still be compiled and tested locally.

## Current validation status
Standalone tests have passed in the authoring environment. The JUCE VST3 target was attempted but not compiled because the environment could not resolve `github.com`. This project ZIP is not an installable VST3 binary.
