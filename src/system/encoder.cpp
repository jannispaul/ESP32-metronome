// encoder.cpp
#include "encoder.h"
#include "pins.h"
#include "config.h"
#include <Arduino.h>

namespace Encoder {

  volatile int clicks = 0;
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  EncoderCallback s_cb = nullptr; // <— NEU

  // Quadratur-Tabelle (Gray-Code)
  static const int8_t table[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
  };

  // --- Debug einschalten: einkommentieren ---
#define ENCODER_DEBUG 1

  static void encoderTask(void*) {
    int last  = (digitalRead(Pins::EncoderCLK) << 1) | digitalRead(Pins::EncoderDT);
    int accum = 0;

#ifdef ENCODER_DEBUG
    unsigned long lastPrintMs = 0;
#endif

    for (;;) {
      int state = (digitalRead(Pins::EncoderCLK) << 1) | digitalRead(Pins::EncoderDT);
      int delta = table[(last << 2) | state];
      last = state;

      if (delta) {
        // *** WICHTIG: Akkumulieren (fehlte in deinem Snippet) ***
        accum += delta;

        // Vollständige Detents „herausteilen“ (kann ±1, ±2, … sein)
        if (abs(accum) >= ENCODER_DETENT_STEPS) {
          int steps = accum / ENCODER_DETENT_STEPS;   // Vorzeichen bleibt erhalten

          portENTER_CRITICAL(&mux);
          clicks += steps;
          portEXIT_CRITICAL(&mux);
          if (s_cb) { s_cb(steps); }                 // <— NEU: Callback aufrufen

          // Rest behalten (0..±(DETENT_STEPS-1))
          accum -= steps * ENCODER_DETENT_STEPS;

#ifdef ENCODER_DEBUG
          // Debug-Ausgabe gedrosselt (max alle 10 ms)
          unsigned long now = millis();
          if (now - lastPrintMs >= 10) {
            // Achtung: printf in Tasks ist ok; nur nicht in ISRs nutzen
            Serial.printf("[ENC] steps=%+d, accum=%+d, clicks=%d, state=%d, delta=%+d\n",
                          steps, accum, clicks, state, delta);
            lastPrintMs = now;
          }
#endif
        }
      }

      // Kurzer, fester Polling-Takt (Debounce)
      vTaskDelay(pdMS_TO_TICKS(ENCODER_DEBOUNCE_MS));
    }
  }

  void init() {
    pinMode(Pins::EncoderCLK, INPUT_PULLUP);
    pinMode(Pins::EncoderDT,  INPUT_PULLUP);
    xTaskCreatePinnedToCore(encoderTask, "Encoder", 2048, nullptr, 2, nullptr, 0);
  }

  int consumeClicks() {
    portENTER_CRITICAL(&mux);
    int c = clicks;
    clicks = 0;
    portEXIT_CRITICAL(&mux);
    return c;
  }
  
  void setCallback(EncoderCallback cb) { s_cb = cb; } // <— NEU
}