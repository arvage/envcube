# EnvCube — Voice Alert Audio Files

The DFPlayer Mini reads MP3 files from a microSD card.
Files must be named `/01.mp3` through `/11.mp3` in the root of the card.

## Track listing

| Track | Filename | Script |
|---|---|---|
| 1  | `/01.mp3` | *"Smoke detected. Please evacuate."* |
| 2  | `/02.mp3` | *"CO2 level is high. Please ventilate."* |
| 3  | `/03.mp3` | *"CO2 level is critical. Ventilate immediately."* |
| 4  | `/04.mp3` | *"Temperature is too high."* |
| 5  | `/05.mp3` | *"Humidity warning."* |
| 6  | `/06.mp3` | *"Poor air quality detected."* |
| 7  | `/07.mp3` | *"High particulate levels detected."* |
| 8  | `/08.mp3` | *"High noise level."* |
| 9  | `/09.mp3` | *"All clear."* |
| 10 | `/10.mp3` | *"WiFi connected."* |
| 11 | `/11.mp3` | *"Setup mode. Connect to EnvCube Setup."* |

## Recording tips

- Record at **44100 Hz, mono, 64 kbps MP3**
- Duration: **2–4 seconds** per clip
- Level: normalise to **-3 dBFS** (loud and clear)
- Avoid background noise or music — clear voice only
- Use a consistent voice/tone across all clips

## Quick recording options

**Option A — macOS `say` command (prototype only):**
```bash
say -v Samantha -r 160 "Smoke detected. Please evacuate." -o 01.aiff
ffmpeg -i 01.aiff -ar 44100 -ac 1 -ab 64k 01.mp3
```

**Option B — ElevenLabs or similar TTS (production):**
Use a consistent voice ID, export as 44100 Hz mono MP3.

**Option C — Record yourself:**
Use Audacity or Voice Memos, export as MP3.

## microSD card setup

1. Format as **FAT32**
2. Copy files to the **root directory** (not a subfolder)
3. Verify filenames are exactly `/01.mp3`, `/02.mp3`, etc.
4. Insert into DFPlayer Mini slot (card face down, contacts up)

## Volume

Default volume: **20/30** (set in `config.h` as `AUDIO_VOLUME`).
Adjust at runtime via `DFPlayer::setVolume(vol)` or MQTT command.
