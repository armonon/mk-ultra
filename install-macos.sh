#!/usr/bin/env bash
# Entropy installer for macOS — builds from source, then installs to the user's
# plugin folders. Run from the project root:  bash install-macos.sh
set -euo pipefail

echo "Entropy — macOS build + install"
command -v cmake >/dev/null || { echo "CMake not found. Install with: brew install cmake"; exit 1; }

echo "Configuring (downloads JUCE on first run)..."
cmake --preset release
echo "Building (this takes a few minutes the first time)..."
cmake --build build/release

ART="build/release/plugin/Entropy_artefacts/Release"
VST3_DST="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DST="$HOME/Library/Audio/Plug-Ins/Components"
APP_DST="$HOME/Applications"

mkdir -p "$VST3_DST" "$AU_DST" "$APP_DST"
[ -d "$ART/VST3/Entropy.vst3" ] && { rm -rf "$VST3_DST/Entropy.vst3"; cp -r "$ART/VST3/Entropy.vst3" "$VST3_DST/"; echo "Installed VST3 -> $VST3_DST"; }
[ -d "$ART/AU/Entropy.component" ] && { rm -rf "$AU_DST/Entropy.component"; cp -r "$ART/AU/Entropy.component" "$AU_DST/"; echo "Installed AU -> $AU_DST"; }
[ -d "$ART/Standalone/Entropy.app" ] && { rm -rf "$APP_DST/Entropy.app"; cp -r "$ART/Standalone/Entropy.app" "$APP_DST/"; echo "Installed app -> $APP_DST"; }

echo "Done. Rescan plugins in your DAW. (Unsigned plugins may need a right-click > Open the first time, or a Gatekeeper exception.)"
