# GrainDelay

A creative granular delay audio plug-in built with [JUCE 8](https://juce.com).

* **Formats:** VST3, AU, Standalone
* **Platform:** macOS, universal binary (arm64 + x86_64)
* **Build system:** CMake only — no Projucer, no checked-in JUCE. JUCE 8.0.15 and
  Catch2 v3.9.1 are pinned and fetched by `FetchContent`.

---

## Build

```bash
cmake -B build
cmake --build build
```

The first configure clones JUCE (a few minutes); afterwards it is cached in
`build/_deps`. Artefacts land in:

```
build/GrainDelay_artefacts/Release/
├── AU/GrainDelay.component
├── VST3/GrainDelay.vst3
└── Standalone/GrainDelay.app
```

### Tests

```bash
cd build && ctest --output-on-failure
```

The tests do not only assert - they also leave a listenable source/processed pair
for each interesting case in `build/renders/`. See **Listening to changes** below.

### Useful options

| Option | Default | Meaning |
| --- | --- | --- |
| `-DCMAKE_OSX_ARCHITECTURES=arm64` | `arm64;x86_64` | Single-arch build — roughly twice as fast while developing. |
| `-DGRAINDELAY_BUILD_TESTS=OFF` | `ON` | Skip the Catch2 target (and its download). |
| `-DGRAINDELAY_COPY_PLUGIN_AFTER_BUILD=ON` | `OFF` | Install into `~/Library/Audio/Plug-Ins/` after each build. |

The build is warning-free under `-Wall -Wextra`. Those flags are attached to an
interface target (`graindelay_warnings`) that is linked into our own targets only,
so JUCE's sources are compiled with JUCE's own settings.

### CLion

Open the project folder; CLion picks up `CMakeLists.txt` directly. `Release` is the
default build type, and `compile_commands.json` is exported so the editor sees the
exact compile flags. The `GrainDelay_Standalone` target is directly runnable.

---

## Architecture

```
                  ┌──────────────────────────────────────────┐
   host  ───────► │ GrainDelayAudioProcessor                 │
                  │  · AudioProcessorValueTreeState (APVTS)  │
                  │  · tempo sync via AudioPlayHead          │
                  │  · collectParameters() → POD snapshot    │
                  └──────────────┬───────────────────────────┘
                                 │ setParameters() + process()
                                 ▼
                  ┌──────────────────────────────────────────┐
                  │ graindelay::GrainEngine                  │
                  │                                          │
                  │  scheduler ──► Grain pool (64, fixed)    │
                  │      │              │                    │
                  │   density,          │ each grain: start, │
                  │   jitter            │ length, pitch      │
                  │                     │ ratio, window      │
                  │                     ▼                    │
                  │              ┌─────────────────┐         │
                  │        ┌────►│  DelayBuffer    │────┐    │
                  │        │     │  ring, ≥ 4 s    │    │    │
                  │        │     └─────────────────┘    │    │
                  │        │                            ▼    │
                  │        │                        Σ grains │
                  │        │                            │    │
                  │   softClip ◄── DC block ◄── lowpass ┘    │
                  │        ▲                            │    │
                  │     feedback                     dry/wet │
                  └────────────────────────────────────┼─────┘
                                                       ▼
                                                     output
```

## Listening to changes

A green test tells you the output is finite. It cannot tell you the grains stopped
clicking, or that the new window shape made the tail muddy. So both the test suite
and a dedicated renderer write WAV pairs into `build/renders/`, and a small local
web app plays them back A/B.

```bash
cmake --build build --target graindelay-render
./build/tools/graindelay-render --source clicks    # writes six preset renders
tools/compare/serve.sh                             # indexes them and opens the app
```

`graindelay-render --list` shows the built-in test signals (impulse, click train,
detuned chord, noise burst, sine sweep) and the presets. `--input yourfile.wav`
renders your own material instead.

### It is wired as a send, not an insert

A granular delay normally lives on an aux send, so every preset renders **100 % wet**
and the WAV pair is (dry input, bare return signal). The compare app does the
mixing, the way a console would — a global send amount, 20 % by default, that
persists across renders and reloads.

Three listening modes, all gapless because dry and return play the whole time and
only the gains change:

| Mode | |
| --- | --- |
| **A · Bypass** | dry only |
| **B · Dry + send** | dry + `send` × return — the mix you would print |
| **R · Return** | the return on its own, the way you would solo an aux |

At 20 % the level barely moves (Δ RMS ≈ +0.1 dB) — which is the point of a send, and
why the level-match toggle matters less here than it would on an insert.

Test material lives in `samples/` (git-ignored). `tools/samples.sh` stages real
piano from the Logic / GarageBand content already on the machine:

```bash
tools/samples.sh
./build/tools/graindelay-render --input samples/piano-ambient.wav
```

The compare app (`tools/compare/index.html`) plays **both signals at once** and
crossfades between them, so A/B switching is gapless and you are comparing the same
instant rather than two different playthroughs:

| Key | |
| --- | --- |
| `Space` | play / pause |
| `A` `B` `R` | bypass / dry+send / return only, `X` flips A↔B |
| `[` `]` | send ∓ 5 % |
| `L` | loop |
| `M` | match level — scales the processed signal to the source's RMS, because louder always sounds better and level bias makes A/B worthless |
| `↑` `↓` | previous / next render |

It shows peak and RMS in dBFS for both, the RMS delta, a live overlaid spectrum,
and marks anything past 0 dBFS in red — the feedback path can legitimately exceed
full scale, and it is useful to see exactly where.

The app also accepts dragged-and-dropped WAV pairs named
`<id>--source.wav` / `<id>--processed.wav`, so it works without the server too.

### Why pairing happens in Python

Each producer writes its own WAVs plus a JSON sidecar;
`tools/compare/build_manifest.py` scans the directory and pairs them. Catch2 runs
its cases in parallel processes, so no producer can safely own a shared index file.
The scanner also reads the sample rate out of the WAV header, because the browser's
`decodeAudioData` resamples to the AudioContext rate and loses it.

---

### Source layout

```
Source/
├── ParameterIds.h            parameter ids + note divisions (shared)
├── PluginProcessor.{h,cpp}   APVTS, tempo sync, host glue
├── PluginEditor.{h,cpp}      window layout
├── dsp/
│   ├── DelayBuffer.{h,cpp}   circular buffer, interpolated fractional reads
│   ├── GrainEngine.{h,cpp}   grain pool, scheduler, feedback path
│   └── OnePoleFilter.h       one-pole LP/HP + soft clip
└── gui/
    └── ParameterComponents.{h,cpp}
                              ParameterSlider / Toggle / Choice / Group
tests/
└── GrainEngineTests.cpp      Catch2 suite (offline rendering)
tools/
├── WavWriter.h               dependency-free float32 WAV writer
├── render/RenderMain.cpp     offline renderer: signals x presets -> WAV pairs
└── compare/
    ├── index.html            the A/B compare app
    ├── build_manifest.py     pairs WAVs into manifest.json
    └── serve.sh              index + serve + open
```

`Source/dsp/` is compiled into a separate static library, `GrainDelayDsp`, which
knows nothing about plug-in formats or hosts. That is what lets the tests render
audio offline without instantiating an `AudioProcessor`.

### How the DSP works

The delay buffer is a ring of at least four seconds. Input plus feedback is written
at the write head, one sample per sample.

A **grain** is a short tape head scrubbing through that buffer. When the scheduler
fires (`density` times a second, ± `jitter`), a free slot in the 64-grain pool is
initialised with:

* a **start position**, `delayTime` behind the write head, ± `positionSpray`;
* a **length** of `grainSize` milliseconds;
* a **pitch ratio** of `2^((pitch ± pitchSpray)/12)`, negated with probability
  `reverseProbability` so the grain plays backwards;
* a **gain** of `1/√overlap`, so loudness stays roughly constant as density and
  grain size change.

Each sample, every active grain reads the buffer at its fractional position (linear
interpolation), scaled by a **Tukey window** — a flat top with Hann-shaped tapers,
looked up from a table built in `prepare()`. `alpha = 1` gives a plain Hann window;
lower values give a flatter, more sustained grain. The window is what keeps grains
from clicking at their edges.

The summed grains go through the **feedback path** — a one-pole lowpass at
`filterCutoff`, a fixed 25 Hz DC blocker, then a `tanh` soft clip — and are written
back into the buffer. The soft clip is what makes feedback values up to and beyond
1.0 saturate into a stable, musical limit instead of running away.

**Freeze** simply stops the write head. The grains carry on scanning the last four
seconds of captured audio for ever.

### Realtime constraints

The audio thread must never block. `malloc` can take a lock, and a blocked audio
thread is an audible dropout — so `processBlock` does:

* **no allocation** — every buffer is sized in `prepareToPlay`; the grain pool is a
  fixed `std::array` and a request for a 65th grain is dropped, not allocated;
* **no locks** — parameters are read through `std::atomic<float>*` pointers cached
  in the constructor and copied into a plain-old-data snapshot;
* **no logging, no file or network I/O, no exceptions.**

`delayTime`, `feedback`, `dryWet` and the filter coefficient are smoothed per sample
with `juce::SmoothedValue`. The filter's coefficient needs an `exp()`, which is far
too expensive per sample, so the coefficient is computed once per block and *it* is
what gets smoothed.

The test suite enforces the no-allocation rule rather than trusting it: the test
binary replaces the global `operator new` with a counting version and asserts that
200 consecutive `process()` calls allocate exactly zero times. A positive-control
test asserts that `prepare()` *does* allocate, so the check cannot pass by accident.

---

## Parameters

| Parameter | Range | Notes |
| --- | --- | --- |
| Delay Time | 1 – 4000 ms | Distance behind the write head that grains start from. |
| Tempo Sync | on/off | Uses the host BPM instead of milliseconds. |
| Division | 1/32 – 1/2 | Note value used when sync is on. |
| Grain Size | 5 – 1000 ms | Length of a single grain. |
| Density | 0.5 – 100 /s | How often grains are fired. |
| Jitter | 0 – 1 | Randomises the trigger interval; 0 is a metronome. |
| Pitch | ±24 st | Playback ratio of every grain. |
| Pitch Spray | 0 – 12 st | Random detune per grain. |
| Position Spray | 0 – 500 ms | Random start-point offset per grain. |
| Reverse | 0 – 1 | Probability that a grain plays backwards. |
| Feedback | 0 – 1.2 | Soft-clipped, so > 1 saturates rather than blows up. |
| Filter | 20 Hz – 20 kHz | One-pole lowpass inside the feedback loop. |
| Dry/Wet | 0 – 1 | |
| Freeze | on/off | Stops writing; grains loop the captured audio. |

---

## Notes for a Java developer

A few C++ specifics that the code comments point at:

* **RAII** — objects own their resources and free them in their destructor, which
  runs deterministically when the object goes out of scope. `juce::AudioBuffer` and
  `std::unique_ptr` are the examples here; there is no GC and no `finally`.
* **Ownership** — a value member means "contained"; a `std::unique_ptr` means "owned,
  freed with me"; a reference or raw pointer means "borrowed, someone else's problem".
  In JUCE, `addAndMakeVisible(child)` (reference) borrows, while the pointer overload
  takes ownership.
* **Destruction order** — members are destroyed in *reverse* declaration order. This
  is why every APVTS attachment is declared last in its class: it must die before the
  slider it points at.
* **`noexcept`** — a promise that a function throws nothing. It is how the realtime
  contract is written down in the type system.

## Status / possible next steps

* Custom `LookAndFeel` (the GUI already routes every colour through LookAndFeel ids).
* Stereo spread / per-grain panning.
* Presets beyond the single default program.
* Code signing and notarisation for distribution.
