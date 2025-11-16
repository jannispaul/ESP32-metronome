
#pragma once
#include <Arduino.h>

// Zentrale Steuerung für Dateiverwaltung, Preload/Double-Buffering,
// und den Logik-Task (Core 0). Button-Handling ist ausgelagert (buttons.*),
// Pinbelegung steht in hardware_pins.h.

struct PlayerConfig {
    const char* rootPath;          // z.B. "/"
    uint16_t    startBpm;          // z.B. 120
    uint8_t     startVolumePercent;// z.B. 25 (%), 0..100
};

// Initialisiert Filesystem, Audio, lädt erste Datei und startet LogicTask (Core 0).
void player_setup(const PlayerConfig& cfg);

// Schlanke Loop-Weiterführung (optional) – hält nur das Arduino-Loop aktiv.
void player_loop();
