# Entropy installer for Windows — builds from source, then installs the VST3.
# Run from the project root in PowerShell:  powershell -ExecutionPolicy Bypass -File install-windows.ps1
$ErrorActionPreference = "Stop"

Write-Host "Entropy - Windows build + install"
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "CMake not found. Install Visual Studio 2022/2026 with 'Desktop development with C++' (includes CMake)."
    exit 1
}

Write-Host "Configuring (downloads JUCE on first run)..."
cmake --preset release
Write-Host "Building (takes a few minutes the first time)..."
cmake --build build/release --config Release

$art = "build/release/plugin/Entropy_artefacts/Release"
$vst3Dst = "$env:COMMONPROGRAMFILES\VST3"

if (Test-Path "$art/VST3/Entropy.vst3") {
    New-Item -ItemType Directory -Force -Path $vst3Dst | Out-Null
    if (Test-Path "$vst3Dst/Entropy.vst3") { Remove-Item -Recurse -Force "$vst3Dst/Entropy.vst3" }
    Copy-Item -Recurse "$art/VST3/Entropy.vst3" $vst3Dst
    Write-Host "Installed VST3 -> $vst3Dst (may require an elevated prompt)"
}
$exe = "$art/Standalone/Entropy.exe"
if (Test-Path $exe) { Write-Host "Standalone built at: $exe (copy it wherever you like)" }

Write-Host "Done. Rescan plugins in your DAW."
