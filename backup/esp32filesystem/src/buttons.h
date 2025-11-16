
#pragma once
#include <Arduino.h>
#include "hardware_pins.h"

// Debouncter Button-Handler mit Edge-Erkennung (Press-Events).
// Polling-basiert: in Tasks/Loop zyklisch "poll()" aufrufen.

struct ButtonEvents {
    bool nextPressed = false; // kurzer Press-Event (Rising Edge, entprellt)
    bool mutePressed = false; // kurzer Press-Event (Rising Edge, entprellt)
};

class Buttons {
public:
    // debounceMs: Mindestabstand zwischen zwei validen Press-Events je Button
    explicit Buttons(uint32_t debounceMs = 50)
        : _debounceMs(debounceMs) {}

    void begin() {
        pinMode(BUTTON_NEXT_PIN, INPUT_PULLDOWN);
        pinMode(BUTTON_MUTE_PIN, INPUT_PULLDOWN);
        _lastNextRead = digitalRead(BUTTON_NEXT_PIN);
        _lastMuteRead = digitalRead(BUTTON_MUTE_PIN);
        _lastNextMs = _lastMuteMs = millis();
    }

    ButtonEvents poll() {
        ButtonEvents ev{};
        const uint32_t now = millis();

        // --- NEXT ---
        const int curNext = digitalRead(BUTTON_NEXT_PIN);
        if (curNext != _lastNextRead) {
            _lastNextMs = now;            // Zustandswechsel -> Entprellzeit starten
            _lastNextRead = curNext;
        } else {
            // stabiler Zustand
            if (curNext == HIGH && (now - _lastNextMs) >= _debounceMs) {
                if (!_latchedNextHigh) {
                    ev.nextPressed = true; // Rising Edge (stabil HIGH)
                    _latchedNextHigh = true;
                }
            }
            if (curNext == LOW) {
                _latchedNextHigh = false;  // bereit für nächsten Press
            }
        }

        // --- MUTE ---
        const int curMute = digitalRead(BUTTON_MUTE_PIN);
        if (curMute != _lastMuteRead) {
            _lastMuteMs = now;
            _lastMuteRead = curMute;
        } else {
            if (curMute == HIGH && (now - _lastMuteMs) >= _debounceMs) {
                if (!_latchedMuteHigh) {
                    ev.mutePressed = true;
                    _latchedMuteHigh = true;
                }
            }
            if (curMute == LOW) {
                _latchedMuteHigh = false;
            }
        }

        return ev;
    }

private:
    uint32_t _debounceMs;

    // NEXT intern
    int _lastNextRead = LOW;
    uint32_t _lastNextMs = 0;
    bool _latchedNextHigh = false;

    // MUTE intern
    int _lastMuteRead = LOW;
    uint32_t _lastMuteMs = 0;
    bool _latchedMuteHigh = false;
};
