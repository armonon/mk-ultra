Entropy — compiled Linux binaries (Debug, stripped)

These were built and verified in the development environment with:
  - JUCE 8.0.12, g++ 13.3, CMake
  - The standalone launches and reports "JUCE v8.0.12" (it needs real audio
    hardware + a display to show its window; it ran headless here only to
    confirm it instantiates without crashing).

Contents:
  Standalone/Entropy                         - standalone app (Linux x86-64)
  VST3/Entropy.vst3                          - VST3 plugin (Linux x86-64)

These are LINUX x86-64 binaries. They will NOT run on macOS or Windows — for
those, build from the included source with:  cmake --preset release && cmake --build build/release

To run the standalone on Linux:
  ./Standalone/Entropy
To use the VST3 on Linux, copy Entropy.vst3 to ~/.vst3/ and rescan in your DAW.
