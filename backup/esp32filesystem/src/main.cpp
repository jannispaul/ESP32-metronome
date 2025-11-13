#include <Arduino.h>
#include <vector>
#include "filesystem.h"
#include "audio.h"

#define BUTTON_PIN 32 // Button nach 3V3, interner Pulldown

// Dateiverwaltung
static std::vector<String> wavFiles;
static int currentIndex = -1;

// Aktueller Buffer (gehört der LogicTask; Audio greift nur lesend zu)
static uint8_t* currentData = nullptr;
static size_t   currentSize = 0;

// Button-/Wechsel-Logik
static volatile uint32_t pressCount = 0;   // gesammelte "weiter"-Klicks
static bool wasPlaying = false;            // nur noch für Debug

// RAM-Info (optional)
static void print_ram_info() {
  Serial.printf("💾 Heap frei: %u Bytes", ESP.getFreeHeap());
  Serial.println();
}

// ---- Hilfsfunktionen ----
static void freeCurrentBuffer() {
  if (currentData) {
    Serial.printf("🗑️ RAM frei: %s\n", (currentIndex >= 0 && currentIndex < (int)wavFiles.size())
                                       ? wavFiles[currentIndex].c_str() : "(unknown)");
    free(currentData);
    currentData = nullptr;
    currentSize = 0;
  }
}

static bool loadFileAtIndex(int idx) {
  if (idx < 0 || idx >= (int)wavFiles.size()) return false;
  const String& path = wavFiles[idx];
  Serial.printf("➡️ Lade: %s\n", path.c_str());

  size_t sz = 0;
  uint8_t* buf = fs_load_file(path.c_str(), sz);
  if (!buf) {
    Serial.println("❌ Laden fehlgeschlagen.");
    return false;
  }

  currentData = buf;
  currentSize = sz;
  currentIndex = idx;

  // An Audio übergeben (Audio liest ab Offset 44)
  audio_set_buffer(currentData, currentSize);

  // Optional: Debug
  Serial.printf("✅ Geladen: %u Bytes\n", (unsigned)sz);
  Serial.print("Bytes [0..15]: ");
  for (int i = 0; i < 16 && i < (int)sz; ++i) Serial.printf("%02X ", currentData[i]);
  Serial.println();
  print_ram_info();
  return true;
}

static void onNextRequested() {
  pressCount++; // mehrere Klicks werden akkumuliert
  Serial.printf("🔁 Wechsel angefordert (Summe: %u)\n", (unsigned)pressCount);
}

// ---- LogicTask (Core 0): Button & File-Management ----
static void logic_task(void*) {
  Serial.println("🧠 LogicTask running (Core 0)");
  uint32_t lastPress = 0;
  const uint32_t debounce = 300;

  for (;;) {
    // 1) Button-Polling
    if (digitalRead(BUTTON_PIN) == HIGH && (millis() - lastPress > debounce)) {
      lastPress = millis();
      Serial.println("🟢 Button gedrückt");
      onNextRequested();
    }

    // 2) Playback-Done-Event abholen (non-blocking kurze Wartezeit)
    if (audio_take_playback_done(5)) {
      // Wiedergabe ist gerade zu Ende gegangen
      bool hadRequest = (pressCount > 0);
      uint32_t steps = pressCount;
      pressCount = 0;

      // Alten Buffer erst JETZT freigeben
      freeCurrentBuffer();

      // Falls Wechsel gewünscht, nächste Datei laden
      if (hadRequest && !wavFiles.empty()) {
        int n = (int)wavFiles.size();
        int nextIdx = (currentIndex < 0) ? 0 : ((currentIndex + (int)steps) % n);
        loadFileAtIndex(nextIdx);
        // Start erfolgt automatisch am nächsten vollen Beat durch AudioTask
      }
    }

    // (Optional) Debug: Zustand beobachten
    bool nowPlaying = audio_is_playing();
    if (wasPlaying != nowPlaying) {
      Serial.printf("🎚️ playing=%d\n", (int)nowPlaying);
      wasPlaying = nowPlaying;
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(BUTTON_PIN, INPUT_PULLDOWN);

  if (!fs_init()) {
    Serial.println("❌ LittleFS init fehlgeschlagen.");
    return;
  }
  fs_print_info();
  fs_list_files("/");

  wavFiles = fs_get_all_wav_files("/");
  Serial.printf("\n🔊 WAVs gefunden: %d\n", (int)wavFiles.size());
  for (size_t i = 0; i < wavFiles.size(); ++i) {
    Serial.printf(" %d: %s\n", (int)i + 1, wavFiles[i].c_str());
  }

  // 1) AudioTask starten (stellt Mutex, Queue & Task sicher)
  audio_init();
  Serial.println("🎛️ audio_init done");

  // 2) BPM setzen (Grid-Epoch wird auf "jetzt" gesetzt)
  audio_set_bpm(240.0f); // z. B. 60 BPM = 1000 ms Periodendauer

  // 3) Erste Datei in RAM laden und an Audio übergeben
  if (!wavFiles.empty()) {
    bool ok = loadFileAtIndex(0);
    Serial.printf("🚀 Initial load %s\n", ok ? "OK" : "FAILED");
  } else {
    Serial.println("⚠️ Keine WAV-Dateien gefunden.");
  }

  print_ram_info();

  // 4) LogicTask (Core 0) starten
  xTaskCreatePinnedToCore(logic_task, "LogicTask", 4096, nullptr, 1, nullptr, 0);
}

void loop() {
  // Alles läuft in Tasks
  vTaskDelay(pdMS_TO_TICKS(1000));
}