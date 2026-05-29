// EnvCube — DFPlayer stub (full impl in Step 3)
#include "dfplayer.h"
#include "../config.h"
void DFPlayer::begin() { Serial.println("[DFPlayer] Stub"); }
void DFPlayer::play(uint8_t track) { Serial.printf("[DFPlayer] Play track %u (stub)\n", track); }
void DFPlayer::playForSource(AlertSource source) {
    Serial.printf("[DFPlayer] Alert for source %d (stub)\n", (int)source);
}
