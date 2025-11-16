
#pragma once
#include <Arduino.h>

// Zentrale Hardware-Pinbelegung (Buttons). 
// Hier kannst du die Pins an deine Hardware anpassen, ohne Code in player/buttons anzufassen.
// I2S-Pins bleiben weiterhin in audio.h (unverändert).

constexpr int BUTTON_NEXT_PIN = 32; // Button nach 3V3, interner Pulldown
constexpr int BUTTON_MUTE_PIN = 33; // Button nach 3V3, interner Pulldown
constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;
