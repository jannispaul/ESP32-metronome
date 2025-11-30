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
static bool s_wasPlaying = false;             // nur für Debug
static Buttons s_buttons{Pins::ButtonDebounceMs, Pins::ButtonDebounceLongMs};               // Entprellen wie vorher

// BPM (für Periodenberechnung / Timeout)

static volatile int32_t s_pendingSteps = 0;     // NEU: signed
static uint16_t s_bpm = 120;
static int s_volume = 50;                       // NEU: Volume-Cache

static uint32_t s_holdStartMs = 0; // Zeitpunkt, ab dem hold aktiv ist

// ---- Forward declaration für File-Wechsel (wird weiter unten definiert) ----
static void requestFileSwitch(int steps);

static int mod_wrap(int a, int n) {
    if (n <= 0) return 0;
    int r = a % n;
    return (r < 0) ? (r + n) : r;
}


// --- Presets über LittleFS/JSON -------------------------------------------
struct Preset {
    uint16_t bpm;
    int      volume;     // 0..100
    int      fileIndex;  // 0..(s_wavFiles.size()-1)
};

static const char* kPresetFile = "/presets.json";

// Lies komplette Datei als String
static bool fs_read_string(const char* path, String& out) {
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    out = f.readString();
    f.close();
    return true;
}

// Schreibe String atomar (hier: direkt, ausreichend für kleine Daten)
static bool fs_write_string_atomic(const char* path, const String& data) {
    File f = LittleFS.open(path, "w");
    if (!f) return false;
    size_t n = f.print(data);
    f.flush();
    f.close();
    return n == data.length();
}

// Baue JSON für 3 Presets: { "preset1": {...}, "preset2": {...}, "preset3": {...} }

static String make_presets_json(const Preset& p1, const Preset& p2, const Preset& p3) {
    String s = "{\n";
    s += "  \"preset1\": {\"bpm\":" + String(p1.bpm)
       + ",\"volume\":" + String(p1.volume)
       + ",\"fileIndex\":" + String(p1.fileIndex) + "},\n";
    s += "  \"preset2\": {\"bpm\":" + String(p2.bpm)
       + ",\"volume\":" + String(p2.volume)
       + ",\"fileIndex\":" + String(p2.fileIndex) + "},\n";
    s += "  \"preset3\": {\"bpm\":" + String(p3.bpm)
       + ",\"volume\":" + String(p3.volume)
       + ",\"fileIndex\":" + String(p3.fileIndex) + "}\n";
    s += "}\n";
    return s;
}


// Minimaler JSON-Parser für Integer-Felder innerhalb eines Objekt-Abschnitts
static bool findIntInSection(const String& s, const char* section, const char* key, int& out) {
    int secPos = s.indexOf(String("\"") + section + "\"");
    if (secPos < 0) return false;
    int braceOpen  = s.indexOf('{', secPos);
    if (braceOpen < 0) return false;
    int braceClose = s.indexOf('}', braceOpen);
    if (braceClose < 0) return false;

    String sec = s.substring(braceOpen + 1, braceClose); // Inhalt zwischen { ... }
    int keyPos = sec.indexOf(String("\"") + key + "\"");
    if (keyPos < 0) return false;
    int colon = sec.indexOf(':', keyPos);
    if (colon < 0) return false;

    int i = colon + 1;
    while (i < sec.length() && sec[i] == ' ') i++;
    int j = i;
    if (j < sec.length() && sec[j] == '-') j++; // negative Zahlen (nicht gebraucht, aber robust)
    while (j < sec.length() && isDigit((unsigned char)sec[j])) j++;
    String numStr = sec.substring(i, j);
    if (numStr.length() == 0) return false;

    out = numStr.toInt();
    return true;
}

// Parsen der kompletten presets.json in 3 Presets
static bool parse_presets_json(const String& json, Preset out[3]) {
    bool ok = true;
    const char* names[3] = { "preset1", "preset2", "preset3" };
    for (int i = 0; i < 3; ++i) {
        int bpm = 120, vol = 50, idx = 0;
        ok &= findIntInSection(json, names[i], "bpm", bpm);
        ok &= findIntInSection(json, names[i], "volume", vol);
        ok &= findIntInSection(json, names[i], "fileIndex", idx);
        out[i].bpm      = (uint16_t)((bpm < 1) ? 1 : bpm);
        out[i].volume   = (vol < 0) ? 0 : (vol > 100 ? 100 : vol);
        out[i].fileIndex= idx;
    }
    return ok;
}

// Wendet ein Preset an: BPM, Volume, Dateiwechsel (über vorhandene Logik)
static void applyPreset(const Preset& p) {
    // 1) BPM
    s_bpm = (p.bpm < 1) ? 1 : p.bpm;
    audio_set_bpm(s_bpm);
    audio_commit_bpm_now(s_bpm); // NEU: sofortiger Commit
    audio_reset_beat_sync_now(); // NEU: verhindert Doppelbeats
    Gui::postBPM(s_bpm);
    Serial.printf("🎚️ BPM gesetzt: %d\n", s_bpm);

    // 2) Volume
    s_volume = (p.volume < 0) ? 0 : (p.volume > 100 ? 100 : p.volume);
    audio_set_volume(s_volume);
    Gui::postVolume(s_volume);
    Serial.printf("🔊 Volume gesetzt: %d%%\n", s_volume);

    // 3) Dateiwechsel
    if (s_wavFiles.empty()) {
        Serial.println("⚠️ Keine WAV-Dateien vorhanden – Dateiwechsel übersprungen.");
        return;
    }
    int target = p.fileIndex;
    if (target < 0 || target >= (int)s_wavFiles.size()) {
        Serial.printf("⚠️ FILE Index %d außerhalb des Bereichs 0..%d – ignoriert.\n",
                      target, (int)s_wavFiles.size() - 1);
        return;
    }
    int current = (s_currentIndex < 0) ? 0 : s_currentIndex;
    int delta   = target - current;

    // Vorschau in GUI
    Gui::postFile(s_wavFiles[target], target, (int)s_wavFiles.size());
    // Wechsel über vorhandenen Mechanismus (Hold/Preload etc.)
    requestFileSwitch(delta);

    Serial.printf("📁 Dateiwechsel angefordert: %d → %d (%+d) [%s]\n",
                  current, target, delta, s_wavFiles[target].c_str());
    Serial.printf("🎛️ Preset angewendet: BPM=%d VOL=%d%% FILE=%d (%s)\n",
                  s_bpm, s_volume, target, s_wavFiles[target].c_str());
}

// Speichert aktuelles Setting in presets.json unter ID (1..3)
static bool savePreset(int id) {
    if (id < 1 || id > 3) return false;

    Preset now {
        (uint16_t)((s_bpm < 1) ? 1 : s_bpm),
        (s_volume < 0) ? 0 : (s_volume > 100 ? 100 : s_volume),
        (s_currentIndex < 0) ? 0 : s_currentIndex
    };

    // Vorhandene Datei laden (falls vorhanden), sonst Defaults
    Preset arr[3] = {
        Preset{120, 50, 0},
        Preset{120, 50, 0},
        Preset{120, 50, 0},
    };

    String json;
    if (LittleFS.exists(kPresetFile) && fs_read_string(kPresetFile, json)) {
        if (!parse_presets_json(json, arr)) {
            Serial.println("⚠️ presets.json existiert, konnte aber nicht vollständig geparst werden – verwende Defaults.");
        }
    }

    arr[id - 1] = now; // gewähltes Preset überschreiben

    String out = make_presets_json(arr[0], arr[1], arr[2]);
    if (!fs_write_string_atomic(kPresetFile, out)) {
        Serial.println("❌ Preset-Datei schreiben fehlgeschlagen.");
        return false;
    }

    Serial.printf("💾 Preset %d gespeichert: BPM=%d VOL=%d%% FILE=%d\n",
                  id, now.bpm, now.volume, now.fileIndex);
    return true;
}

// Lädt ein Preset aus presets.json und wendet es an
static bool loadPreset(int id) {
    if (id < 1 || id > 3) return false;

    if (!LittleFS.exists(kPresetFile)) {
        Serial.println("⚠️ /presets.json nicht vorhanden – keine Presets geladen.");
        return false;
    }
    String json;
    if (!fs_read_string(kPresetFile, json)) {
        Serial.println("❌ presets.json konnte nicht gelesen werden.");
        return false;
    }

    Preset arr[3];
    if (!parse_presets_json(json, arr)) {
        Serial.println("❌ presets.json Parsing fehlgeschlagen.");
        return false;
    }

    Serial.printf("📖 Preset %d geladen: BPM=%d VOL=%d%% FILE=%d\n",
                  id, arr[id - 1].bpm, arr[id - 1].volume, arr[id - 1].fileIndex);

    applyPreset(arr[id - 1]);
    return true;
}


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
return mod_wrap(base + (int)s_pendingSteps, (int)s_wavFiles.size());
}

static bool loadFileTo(uint8_t*& outPtr, size_t& outSize, int idx) {
    
    if (idx < 0 || idx >= (int)s_wavFiles.size()) {
        Serial.println("❌ Ungültiger Index für WAV-Datei!");
        return false;
    }

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

// Request mit signed steps
static void requestFileSwitch(int steps) {
  if (steps == 0) return;
  s_pendingSteps += steps;
  Serial.printf("🔁 Wechsel angefordert (Summe: %d)\n", (int)s_pendingSteps);
  audio_set_hold(true);
  s_holdStartMs = millis();
  preloadNextIfNeeded();
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
    Gui::postFile(s_wavFiles[idx], idx, (int)s_wavFiles.size());
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
static void onButtonEnc()      { 
    Gui::nextFocus(); 
    Serial.println("Button Enc: Fokus gewechselt");
 }      // Short

static void onButtonEncLong()  {          // Long -> Mute toggle
  bool m = audio_is_muted();
  audio_mute(!m);
  Gui::setMuted(!m);                     // „OFF“ an/aus
  Serial.printf("Mute toggled -> %s\n", !m ? "ON" : "OFF");
  // TODO (optional): Gui::setMuted(!m);
}

static void onButton1()      { 
    Serial.println("Button 1 pressed → Lade Preset 1");
    if (!loadPreset(1)) Serial.println("⚠️ Preset 1 nicht verfügbar.");
}
static void onButton1Long()  { 
    Serial.println("Button 1 Long Press → Speichere Preset 1");
    (void)savePreset(1);
}

static void onButton2()      { 
    Serial.println("Button 2 pressed → Lade Preset 2");
    if (!loadPreset(2)) Serial.println("⚠️ Preset 2 nicht verfügbar.");
}
static void onButton2Long()  { 
    Serial.println("Button 2 Long Press → Speichere Preset 2");
    (void)savePreset(2);
}

static void onButton3()      { 
    Serial.println("Button 3 pressed → Lade Preset 3");
    if (!loadPreset(3)) Serial.println("⚠️ Preset 3 nicht verfügbar.");
}
static void onButton3Long()  { 
    Serial.println("Button 3 Long Press → Speichere Preset 3");
    (void)savePreset(3);
}



static void onEncoderDelta(int delta) {
  if (delta == 0) return;
  switch (Gui::getFocus()) {
    case Gui::Focus::BPM: {
      s_bpm += delta;
      if (s_bpm < 1) s_bpm = 1;
      audio_set_bpm(s_bpm);
      Gui::postBPM(s_bpm);
      Serial.printf("[ENC] BPM delta=%+d -> %d\n", delta, s_bpm);
    } break;

    case Gui::Focus::VOL: {
      s_volume += delta;
      if (s_volume < 0) s_volume = 0;
      if (s_volume > 100) s_volume = 100;
      audio_set_volume(s_volume);
      Gui::postVolume(s_volume);
      Serial.printf("[ENC] VOL delta=%+d -> %d%%\n", delta, s_volume);
    } break;


    case Gui::Focus::FILE: {
        s_pendingSteps += delta;
        Serial.printf("🔁 Wechsel angefordert (Summe: %d)\n", (int)s_pendingSteps);
        // Kein Preload hier!
        // Nur GUI-Vorschau:
        int t = computeTargetIndex();
        if (t >= 0 && t < (int)s_wavFiles.size()) {
            Gui::postFile(s_wavFiles[t], t, (int)s_wavFiles.size());
        }
    } break;

  }
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
            if (!playingNow && s_nextReady && s_pendingSteps != 0) {
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

        
        if (s_pendingSteps != 0 && !audio_get_hold()) {
            audio_set_hold(true);
            s_holdStartMs = millis();
            preloadNextIfNeeded(); // wie bisher
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
    s_volume = cfg.startVolumePercent;
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
        Gui::showInitialVolume(s_volume);
        Gui::postVolume(s_volume);                          // NEU
        //vTaskDelay(pdMS_TO_TICKS(500)); 
        Gui::startTask();                  // danach Task starten
        Gui::setMuted(audio_is_muted());       // „OFF“ anzeigen, falls aktuell gemutet
        Gui::setFocus(Gui::Focus::BPM);                     // Start auf BPM

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
