//player.cpp
#include <Arduino.h>
#include <vector>
#include "filesystem.h"
#include "audio.h"
#include "player.h"
#include "buttons.h"
#include "encoder.h"
#include "gui/gui.h"   // <— GUI-API (init, startTask, postBPM, setMuted)

// --------------------------- Interne Zustände ---------------------------
static std::vector<String> s_wavFiles;
static int      s_currentIndex = -1;
static uint8_t* s_currentData  = nullptr;
static size_t   s_currentSize  = 0;

// Double-Buffering für Preload
static uint8_t* s_nextData  = nullptr;
static size_t   s_nextSize  = 0;
static int      s_nextIndex = -1;
static bool     s_nextReady = false;
static bool     s_nextLoading = false;

// Buttons / Wechsel-Logik
static volatile uint32_t s_pendingSteps = 0;  // gesammelte "weiter"-Klicks
static bool s_wasPlaying = false;             // nur für Debug
static Buttons s_buttons{Pins::ButtonDebounceMs, Pins::ButtonDebounceLongMs};               // Entprellen wie vorher

// BPM (für Periodenberechnung / Timeout)
static uint16_t s_bpm = 120; // Startwert (ganzzahlig)
static uint32_t s_holdStartMs = 0; // Zeitpunkt, ab dem hold aktiv ist

// --------------------------- Utils ---------------------------
static void print_ram_info(const char* tag = nullptr) {
    if (tag) Serial.printf("\xF0\x9F\x92\xBE Heap frei [%s]: %u Bytes\n", tag, ESP.getFreeHeap());
    else     Serial.printf("\xF0\x9F\x92\xBE Heap frei: %u Bytes\n", ESP.getFreeHeap());
}

static void freeBuffer(uint8_t*& ptr, size_t& sz) {
    if (ptr) { free(ptr); ptr = nullptr; sz = 0; }
}

static void freeCurrentBuffer() {
    if (s_currentData) {
        const char* name = (s_currentIndex >= 0 && s_currentIndex < (int)s_wavFiles.size())
                           ? s_wavFiles[s_currentIndex].c_str() : "(unknown)";
        Serial.printf("\xF0\x9F\x97\x91\xEF\xB8\x8F RAM frei (current): %s\n", name);
        freeBuffer(s_currentData, s_currentSize);
    }
}

static void freeNextBuffer() {
    if (s_nextData) {
        const char* name = (s_nextIndex >= 0 && s_nextIndex < (int)s_wavFiles.size())
                           ? s_wavFiles[s_nextIndex].c_str() : "(unknown)";
        Serial.printf("\xF0\x9F\x97\x91\xEF\xB8\x8F RAM frei (next): %s\n", name);
        freeBuffer(s_nextData, s_nextSize);
    }
    s_nextIndex = -1; s_nextReady = false; s_nextLoading = false;
}

static int computeTargetIndex() {
    if (s_wavFiles.empty()) return -1;
    int base = (s_currentIndex < 0) ? 0 : s_currentIndex;
    return (base + (int)s_pendingSteps) % (int)s_wavFiles.size();
}

static bool loadFileTo(uint8_t*& outPtr, size_t& outSize, int idx) {
    if (idx < 0 || idx >= (int)s_wavFiles.size()) return false;
    const String& path = s_wavFiles[idx];
    Serial.printf("\xE2\xAC\x87\xEF\xB8\x8F Preload: %s\n", path.c_str());
    size_t sz = 0;
    uint8_t* buf = fs_load_file(path.c_str(), sz); // filesystem.cpp loggt bereits einmal
    if (!buf) { Serial.println("\xE2\x9D\x8C Preload fehlgeschlagen."); return false; }
    outPtr = buf; outSize = sz; return true;
}

static void preloadNextIfNeeded() {
    if (s_wavFiles.empty()) return;
    if (s_pendingSteps == 0) return;
    if (s_nextReady || s_nextLoading) return;
    const int target = computeTargetIndex();
    if (target < 0) return;
    s_nextLoading = true;
    if (s_nextData) freeNextBuffer();
    if (loadFileTo(s_nextData, s_nextSize, target)) {
        s_nextIndex = target; s_nextReady = true;
        Serial.printf("\xE2\x9C\x85 Preload fertig: %s (%u Bytes)\n",
                      s_wavFiles[s_nextIndex].c_str(), (unsigned)s_nextSize);
        print_ram_info("nach Preload");
    }
    s_nextLoading = false;
}

static void handoverToAudio(int idx, uint8_t* data, size_t size) {
    s_currentIndex = idx; s_currentData = data; s_currentSize = size;
    audio_set_buffer(s_currentData, s_currentSize);
    Serial.printf("\xE2\x9C\x85 Geladen (aktiv): %s (%u Bytes)\n",
                  s_wavFiles[s_currentIndex].c_str(), (unsigned)s_currentSize);
    Serial.print("Bytes [0..15]: ");
    for (int i = 0; i < 16 && i < (int)s_currentSize; ++i) Serial.printf("%02X ", s_currentData[i]);
    Serial.println();
    print_ram_info("aktiv");
}

static bool loadFileAtIndex(int idx) {
    if (idx < 0 || idx >= (int)s_wavFiles.size()) return false;
    const String& path = s_wavFiles[idx];
    Serial.printf("\xE2\x9E\xA1\xEF\xB8\x8F Lade (initial): %s\n", path.c_str());
    size_t sz = 0; uint8_t* buf = fs_load_file(path.c_str(), sz);
    if (!buf) { Serial.println("\xE2\x9D\x8C Laden fehlgeschlagen."); return false; }
    handoverToAudio(idx, buf, sz); return true;
}

// Beim Klick: Hold setzen, Zeitpunkt merken, Preload starten
static void onNextRequested() {
    s_pendingSteps++;
    Serial.printf("\xF0\x9F\x94\x81 Wechsel angefordert (Summe: %u)\n", (unsigned)s_pendingSteps);
    audio_set_hold(true);
    s_holdStartMs = millis(); // für "max. 1 Beat" Timeout
    preloadNextIfNeeded();    // Preload sofort starten
}


// ---- Actions called by buttons/encoder ----
static void onButtonEnc()      { Serial.println("Button Enc pressed"); }      // Short
static void onButtonEncLong()  {                                            // Long -> Mute toggle
  bool m = audio_is_muted();
  audio_mute(!m);
  Gui::setMuted(!m);                     // „OFF“ an/aus
  Serial.printf("Mute toggled -> %s\n", !m ? "ON" : "OFF");
  // TODO (optional): Gui::setMuted(!m);
}
static void onButton1()        { Serial.println("Button 1 pressed"); }
static void onButton1Long()    { Serial.println("Button 1 Long Press"); }
static void onButton2()        { Serial.println("Button 2 pressed"); }
static void onButton2Long()    { Serial.println("Button 2 Long Press"); }
static void onButton3()        { Serial.println("Button 3 pressed"); }
static void onButton3Long()    { Serial.println("Button 3 Long Press"); }
static void onEncoderDelta(int delta) {
  if (delta == 0) return;
  // Schrittweite: 1 BPM pro Encoder-Schritt (bei Bedarf anpassen/accelerate)
  s_bpm += delta;
  if (s_bpm < 1) s_bpm = 1;             // defensiv (keine 0 oder negativen BPM)
  audio_set_bpm(s_bpm);                  // Audiotakt aktualisieren
  Gui::postBPM(s_bpm);                   // Anzeige aktualisieren
  Serial.printf("[ENC] delta=%+d -> BPM=%d\n", delta, s_bpm);
}


// --------------------------- Logic Task (Core 0) ---------------------------

static void logic_task(void*) {
  Serial.println("🧠 LogicTask running (Core 0)");
  s_buttons.begin();
  Encoder::init();


// --- Registrierung der Short + Long Button Aktionen ---
  s_buttons.setOnEnc     (onButtonEnc);
  s_buttons.setOnEncLong (onButtonEncLong);     // <— Mute toggle

  s_buttons.setOnButton1 (onButton1);
  s_buttons.setOnButton1Long(onButton1Long);

  s_buttons.setOnButton2 (onButton2);
  s_buttons.setOnButton2Long(onButton2Long);

  s_buttons.setOnButton3 (onButton3);
  s_buttons.setOnButton3Long(onButton3Long);

  // Encoder-Delta (Drehen)
  Encoder::setCallback(onEncoderDelta);


  for(;;) {
    // Entprellung & Event-Erkennung — ruft intern die Callbacks
    s_buttons.poll();

        

        // 2) Playback-Done-Event: bevorzugter Wechsel nach Ende der Wiedergabe
        if (audio_take_playback_done(5)) {
            bool wantSwitch = (s_pendingSteps > 0);
            if (wantSwitch) {
                if (!s_nextReady) {
                    const int target = computeTargetIndex();
                    if (target >= 0) {
                        if (s_nextData) freeNextBuffer();
                        s_nextLoading = true;
                        if (loadFileTo(s_nextData, s_nextSize, target)) {
                            s_nextIndex = target; s_nextReady = true;
                            Serial.printf("\xE2\x9C\x85 Preload (fallback) fertig: %s (%u Bytes)\n",
                                           s_wavFiles[s_nextIndex].c_str(), (unsigned)s_nextSize);
                            print_ram_info("nach Preload(FB)");
                        }
                        s_nextLoading = false;
                    }
                }

                if (s_nextReady) {
                    freeCurrentBuffer();
                    uint8_t* newData = s_nextData; size_t newSize = s_nextSize; int newIndex = s_nextIndex;
                    s_nextData = nullptr; s_nextSize = 0; s_nextIndex = -1; s_nextReady = false; s_nextLoading = false;
                    handoverToAudio(newIndex, newData, newSize);
                    s_pendingSteps = 0;
                    audio_set_hold(false);
                } else {
                    Serial.println("\xE2\x9A\xA0\xEF\xB8\x8F Wechsel angefordert, aber Preload nicht verfügbar – bleibt auf aktueller Datei.");
                    s_pendingSteps = 0; audio_set_hold(false);
                }
            } else {
                audio_set_hold(false);
            }
        }

        // 3) SOFORT‑SWAP bei Leerlauf & fertigem Preload (Deadlock-Prävention)
        if (audio_get_hold()) {
            const bool playingNow = audio_is_playing();
            if (!playingNow && s_nextReady && s_pendingSteps > 0) {
                freeCurrentBuffer();
                uint8_t* newData = s_nextData; size_t newSize = s_nextSize; int newIndex = s_nextIndex;
                s_nextData = nullptr; s_nextSize = 0; s_nextIndex = -1; s_nextReady = false; s_nextLoading = false;
                handoverToAudio(newIndex, newData, newSize);
                s_pendingSteps = 0; audio_set_hold(false);
            } else {
                uint32_t period_ms = audio_get_period_ms();
                if (millis() - s_holdStartMs > period_ms) { audio_set_hold(false); }
            }
        }

        // 4) Preload ggf. nachholen, wenn Klicks vorliegen
        if (s_pendingSteps > 0 && !s_nextReady && !s_nextLoading) {
            preloadNextIfNeeded();
        }

        // Optionaler Debug
        bool nowPlaying = audio_is_playing();
        if (s_wasPlaying != nowPlaying) {
            Serial.printf("\xF0\x9F\x8E\x9A\xEF\xB8\x8F playing=%d\n", (int)nowPlaying);
            s_wasPlaying = nowPlaying;
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// --------------------------- Öffentliche API ---------------------------
void player_setup(const PlayerConfig& cfg) {
    if (!fs_init()) { Serial.println("\xE2\x9D\x8C LittleFS init fehlgeschlagen."); return; }
    fs_print_info();
    fs_list_files(cfg.rootPath);

    s_wavFiles = fs_get_all_wav_files(cfg.rootPath);
    Serial.printf("\n\xF0\x9F\x94\x8A WAVs gefunden: %d\n", (int)s_wavFiles.size());
    for (size_t i = 0; i < s_wavFiles.size(); ++i) Serial.printf(" %d: %s\n", (int)i + 1, s_wavFiles[i].c_str());

    // 1) BPM zuerst setzen
    s_bpm = (cfg.startBpm == 0) ? 1 : cfg.startBpm;
    audio_set_bpm(s_bpm);

    // 2) Start-Lautstärke & Mute-Status
    audio_set_volume(cfg.startVolumePercent);
    (void)audio_get_volume(); (void)audio_is_muted();

    // 3) AudioTask starten
    audio_init();
    Serial.println("\xF0\x9F\x8D\x9B\xEF\xB8\x8F audio_init done");

    // 3.1) GUI starten
    if (Gui::init()) {                       // Display & Queue bereitstellen
    //Gui::startTask();                      // GUI-Task (Core 0) starten
    //Gui::postBPM(s_bpm);                   // Initiale BPM anzeigen
    vTaskDelay(pdMS_TO_TICKS(50));    // NEU: kurze Pause für Display-Stabilität
    Gui::showInitialBPM(s_bpm);        // NEU: sofortige Anzeige
    //vTaskDelay(pdMS_TO_TICKS(500)); 
    Gui::startTask();                  // danach Task starten
    Gui::setMuted(audio_is_muted());       // „OFF“ anzeigen, falls aktuell gemutet
    }


    // 4) Erste Datei laden
    if (!s_wavFiles.empty()) {
        bool ok = loadFileAtIndex(0);
        Serial.printf("\xF0\x9F\x9A\x80 Initial load %s\n", ok ? "OK" : "FAILED");
    } else {
        Serial.println("\xE2\x9A\xA0\xEF\xB8\x8F Keine WAV-Dateien gefunden.");
    }

    print_ram_info("setup");

    // 5) LogicTask starten (Core 0)
    xTaskCreatePinnedToCore(logic_task, "LogicTask", 4096, nullptr, 1, nullptr, 0);
}

void player_loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
