
#include <Arduino.h>
#include "player.h"
#include "hardware_pins.h" // nur zur Sichtbarkeit/Klarheit der Pinzuweisung

void setup() {
  Serial.begin(115200);
  delay(200);

  PlayerConfig cfg{};
  cfg.rootPath = "/";              // Wurzel im LittleFS
  cfg.startBpm = 120;               // Start-BPM
  cfg.startVolumePercent = 15;      // Start-Lautstärke in %

  // Button-Pins sind nun zentral in hardware_pins.h definiert (BUTTON_NEXT_PIN, BUTTON_MUTE_PIN)
  player_setup(cfg);
}

void loop() { player_loop(); }
