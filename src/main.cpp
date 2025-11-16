#include <Arduino.h>
#include "system/player.h"

// Minimaler Einstiegspunkt, nah an deiner bisherigen main.cpp.
// Filesystem/Buttons/Encoder-Handling sind extern; Core-Zuordnung erfolgt im Player.

void setup() {
  Serial.begin(115200);
  delay(200);

  PlayerConfig cfg{};
  cfg.rootPath = "/";            // Wurzelverzeichnis (LittleFS)
  cfg.startBpm = 120;            // Start-BPM
  cfg.startVolumePercent = 15;   // Start-Lautstärke in %

  player_setup(cfg);
}

void loop() {
  player_loop();
}