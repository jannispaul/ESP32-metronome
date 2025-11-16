#pragma once
#include <Arduino.h>
#include "pins.h"

// Button-/Encoder-Handler mit robuster Entprellung (entprellter Zustand),
// Short- & Long-Press für alle Buttons.
// - Enc = active-LOW (Taster nach GND -> INPUT_PULLUP)
// - Button1..3 = active-HIGH (Taster nach 3V3 -> INPUT_PULLDOWN)
// Zeiten kommen ausschließlich aus Pins::{ButtonDebounceMs, ButtonDebounceLongMs}

using ButtonCallback = void(*)();

struct ButtonEvents {
  bool encPressed = false;  // Short-Press (bei entprellter RELEASE-Transition, wenn kein Long)
  bool b1Pressed  = false;
  bool b2Pressed  = false;
  bool b3Pressed  = false;

  bool encLong    = false;  // Long-Press (einmalig während gedrückt)
  bool b1Long     = false;
  bool b2Long     = false;
  bool b3Long     = false;
};

class Buttons {
public:
  explicit Buttons(uint32_t debounceMs = Pins::ButtonDebounceMs,
                   uint32_t longMs     = Pins::ButtonDebounceLongMs)
    : _debounceMs(debounceMs), _longMs(longMs) {}

  // Short-Press Callbacks
  void setOnEnc     (ButtonCallback cb) { _onEnc = cb; }
  void setOnButton1 (ButtonCallback cb) { _onB1  = cb; }
  void setOnButton2 (ButtonCallback cb) { _onB2  = cb; }
  void setOnButton3 (ButtonCallback cb) { _onB3  = cb; }

  // Long-Press Callbacks
  void setOnEncLong     (ButtonCallback cb) { _onEncLong = cb; }
  void setOnButton1Long (ButtonCallback cb) { _onB1Long  = cb; }
  void setOnButton2Long (ButtonCallback cb) { _onB2Long  = cb; }
  void setOnButton3Long (ButtonCallback cb) { _onB3Long  = cb; }

  void begin() {
    // Enc: active-LOW (nach GND) -> Pullup
    pinMode(Pins::ButtonEnc, INPUT_PULLUP);
    initState(_enc, /*activeHigh=*/false, digitalRead(Pins::ButtonEnc));

    // B1..B3: active-HIGH (nach 3V3) -> Pulldown
    pinMode(Pins::Button1, INPUT_PULLDOWN);
    initState(_b1, /*activeHigh=*/true, digitalRead(Pins::Button1));

    pinMode(Pins::Button2, INPUT_PULLDOWN);
    initState(_b2, /*activeHigh=*/true, digitalRead(Pins::Button2));

    pinMode(Pins::Button3, INPUT_PULLDOWN);
    initState(_b3, /*activeHigh=*/true, digitalRead(Pins::Button3));
  }

  // poll() liefert Events zurück und ruft (falls gesetzt) die Callbacks direkt auf.
  ButtonEvents poll() {
    ButtonEvents ev{};

    handleOne(Pins::ButtonEnc, _enc, ev.encPressed, ev.encLong);
    handleOne(Pins::Button1,   _b1,  ev.b1Pressed,  ev.b1Long);
    handleOne(Pins::Button2,   _b2,  ev.b2Pressed,  ev.b2Long);
    handleOne(Pins::Button3,   _b3,  ev.b3Pressed,  ev.b3Long);

    // --- Callbacks (falls gesetzt) ---
    // Reihenfolge: zuerst Long (falls ausgelöst), dann Short (nur wenn kein Long in der Phase)
    if (ev.encLong    && _onEncLong) _onEncLong();
    if (ev.encPressed && !_enc.longFired && _onEnc) _onEnc();

    if (ev.b1Long     && _onB1Long)  _onB1Long();
    if (ev.b1Pressed  && !_b1.longFired && _onB1) _onB1();

    if (ev.b2Long     && _onB2Long)  _onB2Long();
    if (ev.b2Pressed  && !_b2.longFired && _onB2) _onB2();

    if (ev.b3Long     && _onB3Long)  _onB3Long();
    if (ev.b3Pressed  && !_b3.longFired && _onB3) _onB3();

    return ev;
  }

private:
  struct BtnState {
    // Hardware-/Logikparameter
    bool activeHigh = true;        // Logikpegel für "gedrückt" (HIGH für B1..B3, LOW für Enc)

    // Entprellung
    int  raw = LOW;                // aktueller Raw-Read
    int  debounced = LOW;          // entprellter Zustand
    int  lastStable = LOW;         // letzter entprellter Zustand
    uint32_t lastChangeMs = 0;     // letzte Zeit, zu der raw != debounced war (Start der Entprellung)

    // Press-Phase
    uint32_t pressStartMs = 0;     // Zeitpunkt, ab dem entprellt ACTIVE wurde
    bool longFired = false;        // Long in dieser Phase bereits ausgelöst?
  };

  void initState(BtnState& s, bool activeHigh, int initialRead) {
    s.activeHigh   = activeHigh;
    s.raw          = initialRead;
    s.debounced    = initialRead;
    s.lastStable   = initialRead;
    s.lastChangeMs = millis();
    s.pressStartMs = 0;
    s.longFired    = false;
  }

  // Robuste Entprellung mit sauberem debounced-State und Events auf Transitionen.
  // - On debounced -> ACTIVE: beginne Press-Phase (pressStartMs, longFired=false)
  // - While ACTIVE: wenn (now - pressStartMs) >= _longMs und long noch nicht gefeuert -> long event
  // - On debounced -> INACTIVE: wenn zuvor ACTIVE ohne long -> short event
  void handleOne(uint8_t pin, BtnState& s, bool& outShort, bool& outLong) {
    const uint32_t now = millis();
    const int ACTIVE   = s.activeHigh ? HIGH : LOW;
    const int INACTIVE = s.activeHigh ? LOW  : HIGH;

    // 1) Rohzustand lesen
    const int cur = digitalRead(pin);

    // 2) Entprellung: bei Änderung raw != debounced -> Debounce-Fenster neu starten
    if (cur != s.raw) {
      s.raw = cur;
      s.lastChangeMs = now;
    }

    // Wenn das Debounce-Fenster abgelaufen ist, übernimmt debounced den raw-Wert
    if ((now - s.lastChangeMs) >= _debounceMs && s.debounced != s.raw) {
      s.debounced = s.raw;

      // 3) Übergangsereignisse auf entprelltem Zustand
      if (s.debounced == ACTIVE) {
        // INACTIVE -> ACTIVE (Press beginnt)
        s.pressStartMs = now;
        s.longFired = false;
      } else {
        // ACTIVE -> INACTIVE (Release)
        // Short nur, wenn in dieser Phase kein Long gefeuert wurde
        if (!s.longFired) {
          outShort = true;
        }
      }

      s.lastStable = s.debounced;
    }

    // 4) Long-Press prüfen (während entprellt ACTIVE)
    if (s.debounced == ACTIVE && !s.longFired) {
      if ((now - s.pressStartMs) >= _longMs) {
        s.longFired = true;
        outLong = true;
      }
    }
  }

  const uint32_t _debounceMs;
  const uint32_t _longMs;

  // Callbacks (können nullptr sein)
  ButtonCallback _onEnc     = nullptr, _onEncLong = nullptr;
  ButtonCallback _onB1      = nullptr, _onB1Long  = nullptr;
  ButtonCallback _onB2      = nullptr, _onB2Long  = nullptr;
  ButtonCallback _onB3      = nullptr, _onB3Long  = nullptr;

  // Zustände
  BtnState _enc;
  BtnState _b1, _b2, _b3;
};