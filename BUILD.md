# Building Entropy

This walks you from a fresh clone to a running plugin. JUCE (8.0.12) and Catch2
are fetched automatically by CMake on first configure — you don't install them
yourself.

## 1. Prerequisites

You need a C++20 compiler and CMake 3.22+.

- **macOS**: install Xcode from the App Store, then the command-line tools:
  `xcode-select --install`. Install CMake with `brew install cmake`.
- **Windows**: install Visual Studio 2022 or 2026 (Community is fine) with the
  "Desktop development with C++" workload. CMake ships with it.
- **Linux**: install a toolchain and JUCE's system dependencies:
  ```bash
  sudo apt-get update
  sudo apt-get install -y build-essential cmake \
    libasound2-dev libjack-jackd2-dev libcurl4-openssl-dev \
    libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev \
    libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev \
    libxrender-dev libwebkit2gtk-4.1-dev libglu1-mesa-dev
  ```

You also need internet access on the first configure (to fetch JUCE).

## 2. Configure and build

From the project root:

```bash
cmake --preset debug
cmake --build build/debug
```

The first configure downloads JUCE and Catch2 and will take a few minutes.
Subsequent builds are fast.

For an optimized build, swap `debug` for `release`:

```bash
cmake --preset release
cmake --build build/release
```

## 3. Run it without a DAW

The standalone target opens a normal app window with live audio:

```bash
# macOS
open "build/debug/plugin/Entropy_artefacts/Debug/Standalone/Entropy.app"

# Linux
./build/debug/plugin/Entropy_artefacts/Debug/Standalone/Entropy

# Windows
build\debug\plugin\Entropy_artefacts\Debug\Standalone\Entropy.exe
```

(The exact path can vary slightly by generator; if it's not there, search the
build folder for `Entropy` with a `.app`, `.exe`, or no extension.)

The VST3/AU are copied to your system plugin folders automatically
(`COPY_PLUGIN_AFTER_BUILD` is on), so they should appear in your DAW after a
rescan.

## 4. Run the tests (optional)

```bash
cmake --preset ci
cmake --build --preset ci
ctest --preset ci
```

## 5. Develop with live visual feedback (VS Code)

Install the *CMake Tools* and *C/C++* extensions, open the folder, and use the
tasks in `.vscode/tasks.json`: "Build + Run Standalone" rebuilds and relaunches
the app so you see and hear edits immediately. F5 ("Debug Standalone") runs it
under the debugger.

## Troubleshooting first-build errors

Because the project was assembled without an on-hand JUCE compile, a couple of
small errors may surface on your first build. Here are the most likely ones and
their fixes.

| Symptom | Cause | Fix |
|---|---|---|
| `CMake Error: Could not find ... JUCE` or a download/network failure on configure | No internet on first configure, or a firewall blocking GitHub | Ensure network access, delete the `build/` folder, reconfigure. Or clone JUCE locally and point CPM at it with `-DCPM_JUCE_SOURCE=/path/to/JUCE`. |
| `'createPluginFilter' was not declared` / link error about it | Plugin entry point not seen | It's defined at the bottom of `PluginProcessor.cpp`; make sure that file is in the build (it is, in `plugin/CMakeLists.txt`). |
| `no member named 'FontOptions'` | Very old JUCE | This project targets JUCE 8; the pin in the root `CMakeLists.txt` is `8.0.12`. Don't lower it below 8.0. |
| `performRealOnlyForwardTransform` asserts at runtime about buffer size | FFT scratch buffer too small | `fftData` is sized `2 * kFftSize` in `SpectralFreeze.cpp` — if you change `kOrder`, keep that relationship. |
| `reduceClipRegion` / `ScopedSaveState` not found | Missing GUI include | `BiohazardLookAndFeel.cpp` includes `juce_gui_basics`; ensure the module link in `plugin/CMakeLists.txt` is intact. |
| Warnings treated as errors stop the build (e.g. unused variable) | `juce_recommended_warning_flags` is strict | Either fix the flagged line, or temporarily remove `juce::juce_recommended_warning_flags` from `target_link_libraries` in `plugin/CMakeLists.txt` while iterating. |
| `CallOutBox::launchAsynchronously` signature mismatch | Minor API drift | Current JUCE takes `(content, areaInScreen, parentComponent)`; if your version differs, check the JUCE docs for `CallOutBox` and adjust the call in `PluginEditor.cpp::openModPanel`. |
| Standalone runs but is silent | No audio input routed | In the standalone, open the audio settings (gear/options) and pick an input device; the granular engine needs incoming audio to capture. Or feed it from a DAW as a VST3/AU effect on a track. |

If you hit something not in this table, copy the first compiler error (the first
one matters most — later ones are often cascade noise) and I'll give you the
exact fix.
