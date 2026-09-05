# GrainDelay

A creative granular delay audio plug-in built with [JUCE 8](https://juce.com).
Formats: **VST3**, **AU**, **Standalone**. macOS universal binary (x86_64 + arm64).

## Build

```bash
cmake -B build
cmake --build build
```

Artefacts land in `build/GrainDelay_artefacts/Release/`.

Status: project skeleton (pass-through audio). DSP, GUI and tests follow.
