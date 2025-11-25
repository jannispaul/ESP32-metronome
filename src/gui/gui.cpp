//gui.cpp
#include "gui.h"
#include <Wire.h>
#include <U8g2lib.h>
#include "system/pins.h"

namespace {
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
  QueueHandle_t guiQueue = nullptr;
  TaskHandle_t guiTaskHandle = nullptr;
  int lastShownBPM = -1;

  int lastShownVolume = -1;        // NEU
  String lastShownFile;             // NEU
  int lastShownFileIndex = -1;      // NEU
  int lastFileCount = 0;            // NEU

  volatile bool gMuted = false;   // NEU

  Gui::Focus gFocus = Gui::Focus::BPM; // NEU

// Hilfszeichner: konsistente Fonts
  static void drawHeader(const char* title) {
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(0, 10, title);
    if (gMuted) u8g2.drawStr(80, 10, "OFF"); // rechts oben als Mute-Hinweis
  }

  static void drawValueCentered(const char* val) {
    const int SCREEN_W = 128, SCREEN_H = 64;
    u8g2.setFont(u8g2_font_logisoso24_tf); // konsistent für alle Werte
    int tw = u8g2.getStrWidth(val);
    int th = u8g2.getMaxCharHeight();
    int x = (SCREEN_W - tw) / 2;
    int y = (SCREEN_H + th) / 2 - 2;
    u8g2.drawStr(x, y, val);
  }

  static void drawBPMPage() {
    u8g2.clearBuffer();
    drawHeader("BPM");
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", (lastShownBPM > 0) ? lastShownBPM : 1);
    drawValueCentered(buf);
    u8g2.sendBuffer();
  }


static void drawFilePage() {
    u8g2.clearBuffer();
    drawHeader("File");

    // Nur Basisname ohne Pfad und ohne Endung (.wav oder .WAV)
    String name = lastShownFile;

    // Entferne führenden Slash, falls vorhanden
    if (name.startsWith("/")) {
        name.remove(0, 1);
    }

    // Entferne Dateiendung
    int dotPos = name.lastIndexOf('.');
    if (dotPos > 0) {
        name.remove(dotPos);
    }


    // Dynamische Schriftwahl für lange Namen
    const uint8_t* font = u8g2_font_logisoso20_tf;
    if (u8g2.getStrWidth(name.c_str()) > 120) font = u8g2_font_logisoso18_tf;
    if (u8g2.getStrWidth(name.c_str()) > 120) font = u8g2_font_logisoso16_tf;

    u8g2.setFont(font);
    int tw = u8g2.getStrWidth(name.c_str());
    int th = u8g2.getMaxCharHeight();
    int x = (128 - tw) / 2;
    int y = (64 + th) / 2 - 2;
    u8g2.drawStr(x, y, name.c_str());
    u8g2.sendBuffer();

}


  static void drawVolPage() {
    u8g2.clearBuffer();
    drawHeader("Vol");
    char buf[16];
    int v = (lastShownVolume >= 0) ? lastShownVolume : 0;
    snprintf(buf, sizeof(buf), "%d%%", v);
    drawValueCentered(buf);
    u8g2.sendBuffer();
  }


  static void redraw() {
    switch (gFocus) {
      case Gui::Focus::BPM: drawBPMPage(); break;
      case Gui::Focus::FILE: drawFilePage(); break;
      case Gui::Focus::VOL: drawVolPage(); break;
    }
  }


  void drawBPM(int bpm) {
    u8g2.clearBuffer();

    // Label "BPM" oben links
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(0, 10, "BPM");

    // NEU: "OFF" unter dem "BPM"-Label, wenn gemutet
    if (gMuted) {
      u8g2.drawStr(0, 22, "OFF");   // eine Zeile darunter; bei Bedarf Koordinaten justieren
    }


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
    while (lastShownBPM < 0) { vTaskDelay(pdMS_TO_TICKS(10)); } // unverändert

    for (;;) {
      if (xQueueReceive(guiQueue, &ev, wait) == pdTRUE) {
        switch (ev.type) {
          case Gui::Event::BPM_CHANGED:
            if (ev.value != lastShownBPM) lastShownBPM = ev.value;
            break;
          case Gui::Event::VOL_CHANGED:
            lastShownVolume = ev.value;
            break;
          case Gui::Event::FILE_CHANGED:
            // value = index, name via separate setter -> wir verwenden postFile(name, index, count)
            // nichts hier, Name kommt in postFile (siehe unten)
            break;
          case Gui::Event::FOCUS_CHANGED:
            gFocus = static_cast<Gui::Focus>(ev.value);
            break;
          case Gui::Event::MUTE_CHANGED:
            gMuted = (ev.value != 0);
            break;
        }
        redraw();
      }
    }
  }
}



namespace Gui {

bool init() {
  // (A) Kurze Power-On-Stabilisierung
  delay(50);

  // (B) I2C Setup
  Wire.begin(Pins::I2C::SDA, Pins::I2C::SCL);
  Wire.setClock(400000); // Mode

  // (C) U8g2 Setup
  u8g2.setI2CAddress(0x3C << 1);   // falls dein Modul 0x3D hat: (0x3D << 1)
  u8g2.begin();

  // (D) Erstes „Blanking“-Frame, damit beim Start nichts „zerschossen“ aussieht
  u8g2.clearBuffer();
  u8g2.sendBuffer();               // leeres Bild übertragen
  // optional: kleine Pause
  delay(10);

  // (E) Queue mit Länge 1 -> xQueueOverwrite() möglich
  guiQueue = xQueueCreate(1, sizeof(Event));
  return (guiQueue != nullptr);
}

void showInitialBPM(int bpm) {
    if (bpm <= 0) bpm = 1;
    lastShownBPM = bpm;
    drawBPM(lastShownBPM);  // direkte Anzeige ohne Queue
}

  void startTask() {
    xTaskCreatePinnedToCore(guiTask, "GUI", 4096, nullptr, 1, &guiTaskHandle, 0); // Core 0
  }

  
  void setMuted(bool muted) {
      if (!guiQueue) { gMuted = muted; redraw(); return; }
      Event ev{ Event::MUTE_CHANGED, muted ? 1 : 0 };
      xQueueOverwrite(guiQueue, &ev);
    }

  bool isMuted() { return gMuted; }


  void setFocus(Focus f) {
      if (!guiQueue) { gFocus = f; redraw(); return; }
      Event ev{ Event::FOCUS_CHANGED, static_cast<int>(f) };
      xQueueOverwrite(guiQueue, &ev);
    }


  Focus getFocus() { return gFocus; }

  void nextFocus() {
    auto f = static_cast<int>(gFocus);
    f = (f + 1) % 3;
    setFocus(static_cast<Focus>(f));
  }


  void postVolume(int percent) {
    if (!guiQueue) { lastShownVolume = percent; redraw(); return; }
    Event ev{ Event::VOL_CHANGED, percent };
    xQueueOverwrite(guiQueue, &ev);
  }


  bool postBPM(int bpm) {
    if (!guiQueue) return false;
    Event ev{ Event::BPM_CHANGED, bpm };
    // "Letzter Wert gewinnt": überschreibe Eintrag falls Queue belegt
    return xQueueOverwrite(guiQueue, &ev) == pdPASS;
  }


  void postFile(const String& name, int index, int count) {
    lastShownFile = name;
    if (!guiQueue) { redraw(); return; }
    Event ev{ Event::FILE_CHANGED, index };
    xQueueOverwrite(guiQueue, &ev);
  }



void showInitialVolume(int percent) {
    if (percent < 0) percent = 0;
    lastShownVolume = percent;
    drawVolPage(); // direkt die Vol-Seite zeichnen
}


}


 