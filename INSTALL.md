
# Installing Entropy

## Linux (ready to run — no build needed)

A self-contained installer with the precompiled plugin is included. From a terminal:

```bash
bash Entropy-installer-linux.sh
```

It installs, for your user only (no root needed):
- the VST3 to `~/.vst3/Entropy.vst3`
- the standalone app to `~/.local/share/Entropy/` with a launcher at `~/.local/bin/entropy` and an application-menu entry

After installing, rescan plugins in your DAW, and launch the standalone with `entropy` (ensure `~/.local/bin` is on your PATH) or from your app menu.

These Linux binaries were compiled and verified in the build environment (JUCE 8.0.12). They are x86-64 Linux only.

## macOS (builds from source)

There's no prebuilt macOS binary in this package — it has to be compiled on a Mac. From the project root:

```bash
bash install-macos.sh
```

It builds a Release version (downloading JUCE on first run) and installs the VST3, AU, and standalone app to your user plugin/Applications folders. Requires Xcode command-line tools and CMake (`brew install cmake`).

## Windows (builds from source)

No prebuilt Windows binary is included — it has to be compiled on Windows. From the project root in PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File install-windows.ps1
```

It builds a Release version and installs the VST3 to the common VST3 folder. Requires Visual Studio 2022/2026 with the "Desktop development with C++" workload (which includes CMake). Installing to the system VST3 folder may require running PowerShell as Administrator.

## Why only Linux ships prebuilt

A compiled plugin is platform-specific, and the binary can only be built on its own platform. The build environment is Linux, so only the Linux binaries could be precompiled. The macOS and Windows installers build from the included source — which is the same code, verified to compile.
