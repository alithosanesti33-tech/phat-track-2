#!/usr/bin/env bash
# Builds PhatTrack as an Audio Unit, installs it, and validates it for Logic.
# Requires: Xcode + command line tools, CMake (brew install cmake), git.
set -euo pipefail
cd "$(dirname "$0")"

JUCE_PATH="${JUCE_PATH:-$HOME/JUCE}"
if [ ! -f "$JUCE_PATH/CMakeLists.txt" ]; then
  echo "JUCE not found at $JUCE_PATH -- cloning JUCE 8..."
  git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git "$JUCE_PATH"
fi

cmake -B build -S . -DJUCE_PATH="$JUCE_PATH" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target PhatTrack_AU -j

echo
echo "Installed to: ~/Library/Audio/Plug-Ins/Components/PhatTrack.component"
echo "Validating with auval (this is the check Logic does)..."
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -v aufx PhTk OpCa
echo
echo "Done. Restart Logic Pro; if needed, Logic Pro > Settings > Plug-in Manager > Reset & Rescan Selection."
