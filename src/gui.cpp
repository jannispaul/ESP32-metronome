#include "gui.h"
#include <Wire.h>
#include <U8g2lib.h>
#include "system/pins.h"

namespace {
  // SSD1306 128x64, I2C, Full-Buffer, kein Reset-Pin
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

  QueueHandle_t guiQueue = nullptr;
  TaskHandle_t  guiTaskHandle = nullptr;
  int           lastShownBPM = -1;

  // Zeichnet den BPM-Wert groß (≈60% der Fläche) und zentriert
  void drawBPM(int bpm) {
    u8g2.clearBuffer();

    // Kleines Label oben links
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(0, 10, "BPM");

    // Zielrechteck (60% der Displayfläche), zentriert
    const int SCREEN_W = 128;
    const int SCREEN_H = 64;
    const float scale  = 0.60f;
    const int W  = int(SCREEN_W * scale);
    const int H  = int(SCREEN_H * scale);
    const int x0 = (SCREEN_W - W) / 2;
    const int y0 = (SCREEN_H - H) / 2;

    // Font-Kandidaten (Ziffernfonts = *_tn → schnell & scharf)
    const uint8_t* fontCandidates[] = {
      u8g2_font_logisoso46_tn,
      u8g2_font_logisoso38_tn,
      u8g2_font_logisoso28_tn,
      u8g2_font_logisoso24_tn
    };
    const uint8_t numFonts = sizeof(fontCandidates) / sizeof(fontCandidates[0]);

    // Prüft, ob die Schrift in den Zielbereich passt
    auto fits = [&](const uint8_t* font) -> bool {
      u8g2.setFont(font);
      char buf[5];
      snprintf(buf, sizeof(buf), "%d", bpm);
      int tw = u8g2.getStrWidth(buf);
      int th = u8g2.getMaxCharHeight();
      return (tw <= W) && (th <= H);
    };

    // Größtmögliche passende Schrift wählen
    const uint8_t* sel = fontCandidates[numFonts - 1];
    for (uint8_t i = 0; i < numFonts; ++i) {
      if (fits(fontCandidates[i])) { sel = fontCandidates[i]; break; }
    }

    u8g2.setFont(sel);
    char num[5];
    snprintf(num, sizeof(num), "%d", bpm);
    int tw = u8g2.getStrWidth(num);
    int th = u8g2.getMaxCharHeight();
    int x  = x0 + (W - tw) / 2;
    int y  = y0 + (H + th) / 2 - 2; // kleine optische Korrektur

    u8g2.drawStr(x, y, num);
    u8g2.sendBuffer(); // flush
  }

  void guiTask(void*) {
    Gui::Event ev;
    const TickType_t wait = pdMS_TO_TICKS(100);

    if (lastShownBPM >= 0) {
      drawBPM(lastShownBPM); // Erste Anzeige falls vorhanden
    }

    for (;;) {
      // "Last-value-wins": Queue-Länge 1 → wir holen das aktuellste Event
      if (xQueueReceive(guiQueue, &ev, wait) == pdTRUE) {
        if (ev.type == Gui::Event::BPM_CHANGED) {
          if (ev.value != lastShownBPM) {
            lastShownBPM = ev.value;
            drawBPM(lastShownBPM);
          }
        }
      }
    }
  }
}

namespace Gui {

  bool init() {
    // I2C Setup
    Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);
    Wire.setClock(400000);            // Fast Mode

    // U8g2: I2C-Adresse als 8-bit Wert (0x3C oder 0x3D jeweils <<1)
    u8g2.setI2CAddress(0x3C << 1);    // falls dein Modul 0x3D hat: (0x3D << 1)
    u8g2.begin();

    // Queue mit Länge 1 → xQueueOverwrite() möglich
    guiQueue = xQueueCreate(1, sizeof(Event));
    return (guiQueue != nullptr);
  }

  void startTask() {
    xTaskCreatePinnedToCore(guiTask, "GUI", 4096, nullptr, 1, &guiTaskHandle, 0); // Core 0
  }

  bool postBPM(int bpm) {
    if (!guiQueue) return false;
    Event ev{ Event::BPM_CHANGED, bpm };
    // "Letzter Wert gewinnt": überschreibe Eintrag falls Queue belegt
    return xQueueOverwrite(guiQueue, &ev) == pdPASS;
  }
}
