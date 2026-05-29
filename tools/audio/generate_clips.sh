#!/bin/bash
# ============================================================
#  EnvCube — Generate prototype voice clips using macOS `say`
#  Requires: macOS + ffmpeg (brew install ffmpeg)
#
#  Usage: chmod +x generate_clips.sh && ./generate_clips.sh
#  Output: 01.mp3 through 11.mp3 in current directory
# ============================================================

VOICE="Samantha"
RATE=160

declare -a SCRIPTS=(
    ""  # placeholder — tracks start at 1
    "Smoke detected. Please evacuate."
    "C O 2 level is high. Please ventilate."
    "C O 2 level is critical. Ventilate immediately."
    "Temperature is too high."
    "Humidity warning."
    "Poor air quality detected."
    "High particulate levels detected."
    "High noise level."
    "All clear."
    "WiFi connected."
    "Setup mode. Connect to EnvCube Setup network."
)

echo "Generating ${#SCRIPTS[@]} voice clips..."

for i in $(seq 1 11); do
    SCRIPT="${SCRIPTS[$i]}"
    PADDED=$(printf "%02d" $i)
    AIFF="${PADDED}.aiff"
    MP3="${PADDED}.mp3"

    echo "  Track $i: $SCRIPT"
    say -v "$VOICE" -r $RATE "$SCRIPT" -o "$AIFF"
    ffmpeg -y -i "$AIFF" -ar 44100 -ac 1 -ab 64k "$MP3" -loglevel error
    rm "$AIFF"
done

echo ""
echo "Done. Copy 01.mp3–11.mp3 to the root of your FAT32 microSD card."
