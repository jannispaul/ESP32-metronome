#pragma once
#include <stdint.h>

// Zentrale Pinbelegung für ESP32
// Diese Datei vereint die bisherigen Angaben aus pins.h und hardware_pins.h.
// Vorgaben des Projekts:
//  - I2S-Pins bleiben HIER definiert (nicht in audio.h)
//  - Keine Legacy-Namen / Aliases
//  - Buttons heißen: Button1, Button2, Button3 sowie ButtonEnc (Encoder-Taster)
//  - Entprellzeit bleibt erhalten (50 ms)
//  - Hinweis: Button1/2 sind aktiv HIGH mit internem Pulldown nach GND

namespace Pins {

  // ---------------- Buttons ----------------
  // Button1 = ehem. Next (GPIO32), Button2 = ehem. Mute (GPIO33), Button3 = frei (GPIO13)
  // Achtung: aktive HIGH Taster gegen 3V3, interner Pulldown verwenden
  constexpr uint8_t Button1   = 32;  // aktiv HIGH, INPUT_PULLDOWN
  constexpr uint8_t Button2   = 33;  // aktiv HIGH, INPUT_PULLDOWN
  constexpr uint8_t Button3   = 13;  // aktiv HIGH, INPUT_PULLDOWN (falls so beschaltet)
  constexpr uint8_t ButtonEnc = 4;   // Encoder-Taster (mechanisch meist gegen GND -> INPUT_PULLUP)

  // Globale Entprellzeit für Buttons
  constexpr uint32_t ButtonDebounceMs = 50; // ms
  constexpr uint32_t ButtonDebounceLongMs = 250; // ms

  // ---------------- Encoder Signale ----------------
  constexpr uint8_t EncoderCLK = 17;
  constexpr uint8_t EncoderDT  = 16;

  // ---------------- LED ----------------
  constexpr uint8_t LED = 2;

  // ---------------- I2S (Audio AMP) ----------------
  namespace I2S {
    constexpr uint8_t DOUT = 25;
    constexpr uint8_t BCLK = 26;
    constexpr uint8_t LRC  = 27;
  }

  // ---------------- I2C (Display etc.) ----------------
  namespace I2C {
    constexpr uint8_t SDA = 21;
    constexpr uint8_t SCL = 22;
  }

  // ---------------- Compile-time Sanity-Checks ----------------
  static_assert(Button1 != Button2 && Button1 != Button3 && Button2 != Button3,
                "Pins conflict: Button1/2/3 share the same GPIO");
  static_assert(EncoderCLK <= 39 && EncoderDT <= 39 && ButtonEnc <= 39,
                "Ungültiger Encoder-Pin (ESP32: 0..39)");
  static_assert(Button1 <= 39 && Button2 <= 39 && Button3 <= 39,
                "Ungültiger Button-Pin (ESP32: 0..39)");
  static_assert(LED <= 39, "Ungültiger LED-Pin (ESP32: 0..39)");
  static_assert(I2S::DOUT <= 39 && I2S::BCLK <= 39 && I2S::LRC <= 39,
                "Ungültiger I2S-Pin (ESP32: 0..39)");
  static_assert(I2C::SDA <= 39 && I2C::SCL <= 39,
                "Ungültiger I2C-Pin (ESP32: 0..39)");
}