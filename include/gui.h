#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace Gui {
  struct Event {
    enum Type : uint8_t { BPM_CHANGED } type;
    int value; // z.B. BPM
  };

  bool init();                // I2C + Display initialisieren, Queue anlegen
  void startTask();           // GUI-Task auf Core 0 starten
  bool postBPM(int bpm);      // Non-blocking: BPM-Event senden
}