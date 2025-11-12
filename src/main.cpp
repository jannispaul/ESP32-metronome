// main.cpp
#include <Arduino.h>
#include "system/pins.h"
#include "system/config.h"
#include "system/buttons.h"
#include "system/encoder.h"
#include "system/led.h"
#include "gui/gui.h"

// Globale
volatile int BPM = BPM_START;
static volatile bool gMuted = false;  // NEU

static void controlTask(void*) {
  const int STEP = 1;
  int lastShown  = BPM;
  int lastLedBpm = BPM;

  // EncoderBTN (aktiv LOW) Debounce
  bool lastBtn = HIGH;                        // ungedrückt = HIGH
  unsigned long lastChangeMs = 0;
  const unsigned long DB_MS  = 30;

  for (;;) {
    // 1) Encoder-Klicks konsumieren (BPM bleibt einstellbar)
    int delta = Encoder::consumeClicks();
    if (delta != 0) {
      int newBPM = BPM + delta * STEP;
      if (newBPM < BPM_MIN) newBPM = BPM_MIN;
      if (newBPM > BPM_MAX) newBPM = BPM_MAX;
      BPM = newBPM;
    }

    // 2) Encoder-Taster lesen (kurzer Klick -> Toggle Mute)
    bool raw = digitalRead(Pins::EncoderBTN);
    unsigned long now = millis();
    if (raw != lastBtn && (now - lastChangeMs) > DB_MS) {
      lastBtn = raw;
      lastChangeMs = now;
      if (raw == LOW) {
        gMuted = !gMuted;
        LEDTask::setEnabled(!gMuted);  // LED ein/aus
        Gui::setMuted(gMuted);         // GUI: "OFF" an/aus
        Serial.printf("[CTRL] EncoderBTN clicked -> %s\n",
                      gMuted ? "OFF (muted)" : "ON (active)");
      }
    }

    // 3) GUI/LED BPM-Updates sind immer erlaubt (auch im OFF-Zustand)
    if (lastShown != BPM) {
      Gui::postBPM(BPM);    // Anzeige bleibt voll funktionsfähig
      lastShown = BPM;
    }
    if (lastLedBpm != BPM) {
      LEDTask::setBPM(BPM); // LED nutzt Takt (nur sichtbar, wenn enabled)
      lastLedBpm = BPM;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Setup complete, starting tasks...");

  pinMode(Pins::LED, OUTPUT);

  // Deine anderen Buttons:
  pinMode(Pins::Button1, INPUT_PULLDOWN);
  pinMode(Pins::Button2, INPUT_PULLDOWN);
  pinMode(Pins::Button3, INPUT_PULLDOWN);

  // WICHTIG: Encoder-Knopf aktiv LOW
  pinMode(Pins::EncoderBTN, INPUT_PULLUP);

  Encoder::init();
  LEDTask::start();
  Buttons::init();

  if (Gui::init()) {
    Gui::startTask();
    Gui::postBPM(BPM);
    Gui::setMuted(gMuted); // initialer Zustand
  }

  xTaskCreatePinnedToCore(controlTask, "Control", 3072, nullptr, 2, nullptr, 0);
}

void loop() {}