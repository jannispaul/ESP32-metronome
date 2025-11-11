#include <Arduino.h>
#include "system/pins.h"
#include "config.h"
#include "gui.h"

// ===== Globale =====
volatile int encoderClicks = 0;
volatile int BPM = BPM_START;
portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;

// Quadratur-Tabelle (wie gehabt)
static const int8_t table[16] = { 0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0 };

// ===== Encoder Task =====
void encoderTask(void*){
  int lastState = (digitalRead(Pins::EncoderCLK) << 1) | digitalRead(Pins::EncoderDT);
  int accum = 0;
  for(;;){
    int state = (digitalRead(Pins::EncoderCLK) << 1) | digitalRead(Pins::EncoderDT);
    int delta = table[(lastState << 2) | state];
    lastState = state;
    if (delta) {
      accum += delta;
      if (abs(accum) >= ENCODER_DETENT_STEPS) {
        portENTER_CRITICAL(&encoderMux);
        encoderClicks += (accum > 0 ? 1 : -1);
        portEXIT_CRITICAL(&encoderMux);
        accum = 0;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(ENCODER_DEBOUNCE_MS));
  }
}

// ===== LED Task =====
void ledTask(void*){
  int64_t nextBeat = esp_timer_get_time();
  for(;;){
    int localBPM = BPM;
    int64_t period_us = 60000000LL / localBPM;
    int64_t on_us = LED_ON_TIME_MS * 1000LL;

    nextBeat += period_us;
    while (esp_timer_get_time() < nextBeat) vTaskDelay(pdMS_TO_TICKS(1));

    digitalWrite(Pins::LED, HIGH);
    int64_t offTime = esp_timer_get_time() + on_us;
    while (esp_timer_get_time() < offTime) {}
    digitalWrite(Pins::LED, LOW);
  }
}

// ===== Controller Task (BPM-Logik + Events) =====
void controlTask(void*){
  int lastShown = BPM;
  Serial.begin(115200); // optional später deaktivieren

  for(;;){
    int clicks = 0;
    portENTER_CRITICAL(&encoderMux);
    clicks = encoderClicks;
    encoderClicks = 0;
    portEXIT_CRITICAL(&encoderMux);

    if (clicks) {
      BPM = constrain(BPM + clicks, BPM_MIN, BPM_MAX);
    }
    if (lastShown != BPM) {
      if (Serial) { Serial.printf("\rBPM: %d ", BPM); }
      Gui::postBPM(BPM);
      lastShown = BPM;
    }
    vTaskDelay(pdMS_TO_TICKS(25));
  }
}

void setup() {
  // pinMode etc.
  pinMode(Pins::EncoderCLK, INPUT_PULLUP);
  pinMode(Pins::EncoderDT,  INPUT_PULLUP);
  pinMode(Pins::LED, OUTPUT);

  // GUI
  if (Gui::init()) {
    Gui::postBPM(BPM);  // initial
    Gui::startTask();   // Core 0, niedrige Prio
  }

  // Tasks auf Core 0
  xTaskCreatePinnedToCore(encoderTask, "Encoder", 2048, nullptr, 2, nullptr, 0);
  xTaskCreatePinnedToCore(ledTask,     "LED",     2048, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(controlTask, "Control", 3072, nullptr, 1, nullptr, 0);
}

void loop(){}