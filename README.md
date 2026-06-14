# Entropy

Entropy — a real-time granular chaos engine (VST3 / AU / Standalone), built with JUCE 8 and CMake. Granular freeze + scrub, FFT spectral freeze, deep modulation, randomization, saturation, and a toxic-green chaos-symbol visual identity.

It captures incoming audio into a circular buffer and sprays a cloud of windowed grains from a movable read position. Freeze locks the read window so the engine keeps emitting grains from a held moment — a frozen, evolving texture you can pitch-shift, scrub, and smear in real time.

## Features

- **Spectral freeze (FFT)** — a separate STFT path (2048-point FFT, 75% overlap-add, Hann windowed) that holds the magnitude spectrum of the incoming grain cloud and resynthesizes a sustained glassy pad from it, with a shimmer control that adds slow per-bin phase drift so the freeze evolves instead of sitting static. Mixed in by its own amount, with light stereo decorrelation.
- **Global mod scope** — a scrolling oscilloscope of the global modulation source value over time, so you can watch the shape and rate of the source feeding the routable mod amount on each knob.
- **CRT power-on** — opening the plugin plays a brief boot animation: a black veil lifts, a green sweep line travels down the face, and the glow surges then settles. The centre glow also carries a continuous subtle flicker for cathode-tube instability.
- **Biohazard theme** — a custom `BiohazardLookAndFeel` restyles every knob, slider, button, and combo box in one place: near-black background with a toxic-green core glow, weathered dark-metal knob bodies, glowing green value arcs, and a hazard-green palette throughout. The knob faces carry a procedurally generated grunge texture (dark speckle, green corrosion, fine scratches — built once into an image and clipped to each knob circle). A biohazard symbol (drawn as a vector `Path`, no bitmap asset) sits as a faint glowing watermark behind the knob grid, and the centre glow plus watermark pulse in time with the output level. A CRT scanline overlay and edge vignette sit over the whole face for an old-equipment cathode-tube feel. The modulation rings keep green for positive and coral for negative, and each arc leaves a fading glowing trail behind its current position as it moves.
- **Hann-windowed grain pool** — fixed pool of 64 voices, each multiplied by a precomputed Hann window so grain boundaries never click.
- **Live input capture + true freeze** — a circular capture buffer continuously records the input; freezing latches a read anchor and holds the material.
- **Decoupled pitch / position** — grains resample for pitch independently of read position, the defining trait of granular synthesis.
- **Randomize button** — rolls a fresh patch within *musical* ranges (not the full extremes), so the dice always land somewhere usable. Per-knob **lock** toggles keep chosen knobs fixed across rolls.
- **Three-layer modulation** per knob — an **LFO** (rate, depth, four shapes), a **sample-and-hold wobble** (rate, depth), and a routable amount from one **global mod source** (rate + shape). Knobs show base values; modulation rides on top, so host automation and modulation coexist. Runs at a ~100 Hz control rate, sample-rate independent.
- **Shareable `.preset` files** — saved as XML to `~/Documents/GrainFreeze/Presets/`. Save, load from a dropdown, and step prev/next. Presets capture every parameter *and* all modulation settings, since the mod controls live in the same `AudioProcessorValueTreeState`.
- **Per-knob modulation panels** — every knob has an "M" button that pops out a `CallOutBox` panel with that parameter's full modulation controls (LFO rate/depth/shape, sample-and-hold rate/depth, global mod amount) plus per-knob **options**: a bipolar/unipolar toggle, modulation range limits (min/max the modulated value can reach), a skew curve that bends the modulation response, and a reset-to-default button. Everything binds to the APVTS, so it's automatable, saved with presets, and reflected live on the ring. The "M" button itself lights up teal when a knob is actively being modulated.
- **Global mod source** — its own rate and shape controls in the bottom bar, routable into any knob via that knob's Global Amt.
- **Output saturation** — selectable Tube (warm, asymmetric, adds even+odd harmonics), Tape (smooth, compressive), or Hard (clip) character, with drive (1–24×, level-compensated) and dry/wet mix, smoothed to avoid zipper noise, sitting at the end of the chain after the reverb. A live transfer-curve display draws the exact shaping function for the current type and drive (it calls the same `Saturator::transfer` the audio path uses, so it never lies), and a two-channel peak meter with decay and green/amber/red zones shows the post-saturation output level.
- **Live modulation rings** — each knob is wrapped by a ring that sweeps an arc in real time from the knob's base position, showing how much and which direction modulation is moving it: teal for positive, coral for negative, dot at the target. Driven by a 30 Hz UI timer reading atomic offsets from the audio thread, so it's glitch-free and never blocks audio.
- **Reverb tail** — a `juce::dsp::Reverb` wet path mixed back in for space.
- All parameters exposed through `AudioProcessorValueTreeState` (automatable, saved with the session).

## Building

Requires CMake 3.22+ and a C++20 compiler. JUCE (and Catch2 for tests) are fetched automatically via CPM on first configure.

Using the presets:

```bash
cmake --preset debug      # configure
cmake --build build/debug # build all targets
```

Or release:

```bash
cmake --preset release
cmake --build build/release
```

Built plugins are copied to your system plugin folders automatically (`COPY_PLUGIN_AFTER_BUILD`).

## Seeing it live while you code (VS Code)

The fastest visual+audio feedback loop is the **Standalone target** — a real app window with the GUI and live audio.

1. Open the folder in VS Code (install the *CMake Tools* and *C/C++* extensions).
2. It configures on open using `CMakePresets.json`.
3. Run the **Build + Run Standalone** task (`Cmd/Ctrl+Shift+B` builds; the run task relaunches the app). Edit a slider range or DSP line, rebuild, and the app reopens with your change audible immediately.
4. **Debug Standalone** (F5) launches it under the debugger with breakpoints.

`compile_commands.json` is exported so IntelliSense resolves JUCE symbols correctly.

## Tests

```bash
cmake --preset ci
cmake --build --preset ci
ctest --preset ci
```

The suite (Catch2) exercises the engine directly — prepare/ready state, silence with no input, sound after freeze, and a safety check that output stays finite and bounded under extreme settings.

## Continuous integration

`.github/workflows/build.yml` builds and tests on Linux, macOS, and Windows on every push and PR, with CPM dependencies cached.

## Project layout

```
GrainFreeze/
├── CMakeLists.txt            root: fetches JUCE/Catch2, adds subdirs
├── CMakePresets.json         debug / release / ci presets
├── .clang-format             code style
├── cmake/cpm.cmake           CPM package-manager bootstrap
├── .github/workflows/        cross-platform CI
├── .vscode/                  tasks, launch, settings for the live loop
├── plugin/
│   ├── CMakeLists.txt        juce_add_plugin target
│   ├── include/GrainFreeze/  GranularEngine.h, PluginProcessor.h, PluginEditor.h
│   └── source/               GranularEngine.cpp, PluginProcessor.cpp, PluginEditor.cpp
└── test/
    ├── CMakeLists.txt        Catch2 test target
    └── source/               GranularEngineTests.cpp
```

## Where to take it next

- Add a waveform display of the capture buffer with a draggable position cursor (mirrors the browser prototype).
- A second envelope shape option (Tukey / Gaussian) selectable per patch.
- Spectral-freeze mode: FFT the frozen window and resynthesize, for a cleaner sustained pad.
