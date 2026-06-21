#!/usr/bin/env bash
# Build the macOS installer (.pkg) from the universal artefacts and zip it for
# release. Reproduces the v2.x installer: a component package that drops the
# VST3 / AU / Standalone into the system locations, plus a postinstall that
# strips the Gatekeeper quarantine flag so the plugin loads without friction.
#
# Usage: ./package-macos.sh <version>        e.g. ./package-macos.sh 2.2
# Prereq: a universal (x86_64;arm64) Release build in build/universal.
set -euo pipefail

VERSION="${1:?usage: ./package-macos.sh <version>}"
ROOT="$(cd "$(dirname "$0")" && pwd)"
ART="$ROOT/build/universal/plugin/Entropy_artefacts/Release"
WORK="$(mktemp -d)"
PAYROOT="$WORK/payload"
SCRIPTS="$WORK/scripts"
OUT="$ROOT/dist/MK_Ultra_macOS_Installer"
PKG_ID="com.YourCompany.MKUltra"

trap 'rm -rf "$WORK"' EXIT

VST3="$ART/VST3/MK Ultra.vst3"
AU="$ART/AU/MK Ultra.component"
CLAP="$ART/CLAP/MK Ultra.clap"
APP="$ART/Standalone/MK Ultra.app"
for b in "$VST3" "$AU" "$APP"; do
    [ -e "$b" ] || { echo "ERROR: missing artefact: $b (build build/universal first)"; exit 1; }
done
# CLAP is optional -- only install it if the build produced one.
HAVE_CLAP=0; [ -e "$CLAP" ] && HAVE_CLAP=1

# Confirm the artefacts are actually universal before we ship them.
echo "==> Verifying universal slices..."
lipo -archs "$VST3/Contents/MacOS/MK Ultra" | grep -q "x86_64" \
  && lipo -archs "$VST3/Contents/MacOS/MK Ultra" | grep -q "arm64" \
  || { echo "ERROR: VST3 is not universal: $(lipo -archs "$VST3/Contents/MacOS/MK Ultra")"; exit 1; }
echo "    VST3 archs: $(lipo -archs "$VST3/Contents/MacOS/MK Ultra")"

# Stage the payload at the install locations.
echo "==> Staging payload..."
mkdir -p "$PAYROOT/Library/Audio/Plug-Ins/VST3" \
         "$PAYROOT/Library/Audio/Plug-Ins/Components" \
         "$PAYROOT/Library/Audio/Plug-Ins/CLAP" \
         "$PAYROOT/Applications"
cp -R "$VST3" "$PAYROOT/Library/Audio/Plug-Ins/VST3/"
[ "$HAVE_CLAP" = "1" ] && cp -R "$CLAP" "$PAYROOT/Library/Audio/Plug-Ins/CLAP/" && echo "    + CLAP"
cp -R "$AU"   "$PAYROOT/Library/Audio/Plug-Ins/Components/"
cp -R "$APP"  "$PAYROOT/Applications/"

# Postinstall: clear the quarantine xattr so Gatekeeper doesn't block loading.
mkdir -p "$SCRIPTS"
cat > "$SCRIPTS/postinstall" <<'POST'
#!/bin/bash
xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/MK Ultra.vst3" 2>/dev/null || true
xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/Components/MK Ultra.component" 2>/dev/null || true
xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/CLAP/MK Ultra.clap" 2>/dev/null || true
xattr -dr com.apple.quarantine "/Applications/MK Ultra.app" 2>/dev/null || true
exit 0
POST
chmod +x "$SCRIPTS/postinstall"

# Build the component package, then wrap it in a product archive.
echo "==> pkgbuild (component)..."
pkgbuild --root "$PAYROOT" --identifier "$PKG_ID" --version "$VERSION" \
         --install-location "/" --scripts "$SCRIPTS" "$WORK/MKUltra.pkg"

echo "==> productbuild (installer)..."
productbuild --synthesize --package "$WORK/MKUltra.pkg" "$WORK/Distribution.xml"
# Give the installer a real title + declare universal host support.
/usr/bin/sed -i '' \
  -e 's#<installer-gui-script minSpecVersion="1">#<installer-gui-script minSpecVersion="2"><title>MK-ULTRA</title><options customize="never" require-scripts="false" hostArchitectures="x86_64,arm64"/>#' \
  "$WORK/Distribution.xml"
productbuild --distribution "$WORK/Distribution.xml" --package-path "$WORK" "$OUT/MK Ultra Installer.pkg"

# Zip for upload (matches the v2.1 asset name).
echo "==> Zipping installer..."
( cd "$ROOT/dist" && rm -f MK_Ultra_macOS_Installer.zip && \
  ditto -c -k --sequesterRsrc --keepParent "MK_Ultra_macOS_Installer" "MK_Ultra_macOS_Installer.zip" )

echo "==> Done: dist/MK_Ultra_macOS_Installer.zip"
ls -lh "$ROOT/dist/MK_Ultra_macOS_Installer.zip"
