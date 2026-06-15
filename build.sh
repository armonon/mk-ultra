#!/usr/bin/env bash
# One-shot configure + build for Entropy (Debug).
# Usage: ./build.sh [debug|release]   (default: debug)
set -euo pipefail

PRESET="${1:-debug}"

echo "==> Configuring ($PRESET) — first run downloads JUCE, please wait..."
cmake --preset "$PRESET"

echo "==> Building ($PRESET)..."
cmake --build "build/$PRESET"

echo ""
echo "==> Done. Standalone app is under:"
echo "    build/$PRESET/plugin/Entropy_artefacts/"
echo "    (look in the Standalone/ subfolder)"
