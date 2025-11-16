#include "audio.h"
#include "driver/i2s.h"
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"  // µs-Zeitstempel

// ------------------- Telemetrie-Schalter -------------------
#define AUDIO_TELEM           1   // 0=aus, 1=ein (Basis-Logs)
#define AUDIO_TELEM_VERBOSE   1   // 0=aus, 1=zusätzliche Beat-Logs

// ------------------- Konfiguration -------------------
constexpr uint32_t SAMPLE_RATE     = 44100; // ggf. anpassen
constexpr size_t   WAV_HEADER_SIZE = 44;    // Standard-WAV-Header

// ------------------- Zustände -------------------
static TaskHandle_t       s_audioTask    = nullptr;
static SemaphoreHandle_t  s_bufMutex     = nullptr;
static std::atomic<bool>  s_playing{false};
static std::atomic<bool>  s_hold{false};
static uint16_t           s_bpm;                   // integer BPM

static uint8_t* s_buf     = nullptr;              // aktueller WAV-Buffer
static size_t   s_bufSize = 0;

static TickType_t s_periodTicks  = 0;   // Ticks pro Beat
static TickType_t s_epochTicks   = 0;   // Grid-Epoch

static bool s_i2sInstalled = false;
static QueueHandle_t s_doneQueue = nullptr;   // 1-Byte Events

// ------------------- Lautstärke / Mute (NEU) -------------------
// Prozent 0..100, intern als Q15-Faktor (0..32767). 100% => 32767.
static std::atomic<uint8_t> s_volumePercent{100};
static std::atomic<bool>    s_muted{false};
static std::atomic<int32_t> s_volQ15{32767};  // Q15 Faktor (signed 1.15 Fixpunkt)

// ------------------- Telemetrie -------------------
static uint64_t   s_beatCounter                  = 0;
static uint64_t   s_startCounter                 = 0;
static uint64_t   s_missedBeats_total            = 0;
static uint32_t   s_missedBeats_recent           = 0;
static uint64_t   s_missedBeats_dueToLength_total= 0;

static uint32_t   s_lastStartMs       = 0;
static uint32_t   s_lastPlannedMs     = 0;
static int32_t    s_lastDriftMs       = 0;
static int32_t    s_avgDriftMs        = 0;
static const int  DRIFT_AVG_WINDOW    = 8;

static inline uint32_t now_ms() {
  return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

// ------------------- Hilfsfunktionen -------------------
static inline void ensure_mutex() {
  if (!s_bufMutex) {
    s_bufMutex = xSemaphoreCreateMutex();
#if AUDIO_TELEM_VERBOSE
    if (s_bufMutex) Serial.println("🔒 audio: mutex created");
#endif
  }
}
static inline void ensure_done_queue() {
  if (!s_doneQueue) {
    s_doneQueue = xQueueCreate(4, sizeof(uint8_t));
#if AUDIO_TELEM_VERBOSE
    if (s_doneQueue) Serial.println("📮 audio: done-queue created");
#endif
  }
}
static inline TickType_t ms_to_ticks(uint32_t ms) {
  TickType_t t = pdMS_TO_TICKS(ms);
  return (t == 0) ? 1 : t;
}
static inline uint32_t bpm_to_period_ms(uint16_t bpm) {
  if (bpm == 0) bpm = 1;
  return 60000u / bpm;
}

// ------------------- I2S Setup (einmalig) -------------------
static bool i2s_install_once() {
  if (s_i2sInstalled) return true;

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len   = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;

  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) {
    Serial.println("❌ I2S install failed");
    return false;
  }
  i2s_pin_config_t pins = {};
  pins.bck_io_num   = I2S_BCLK;
  pins.ws_io_num    = I2S_LRC;
  pins.data_out_num = I2S_DOUT;
  pins.data_in_num  = I2S_PIN_NO_CHANGE;

  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
    Serial.println("❌ I2S set_pin failed");
    i2s_driver_uninstall(I2S_NUM_0);
    return false;
  }

  i2s_set_clk(I2S_NUM_0, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
  s_i2sInstalled = true;
  Serial.println("✅ I2S ready");
  return true;
}

// ------------------- WAV-Dauer (ms) -------------------
static uint32_t current_wav_duration_ms() {
  if (!s_buf || s_bufSize <= WAV_HEADER_SIZE) return 0;
  size_t monoSamples = (s_bufSize - WAV_HEADER_SIZE) / 2;
  uint32_t ms = (uint32_t)((monoSamples * 1000ULL) / SAMPLE_RATE);
  return (ms == 0) ? 1 : ms;
}

// ------------------- Sample-Skalierung (NEU) -------------------
// Skaliert einen 16-bit-Sample mit Q15-Faktor und Sättigung auf int16.
static inline int16_t scale_q15_sat(int16_t x, int32_t q15) {
  // Fastpath: q15==32767 ≙ 100% (nahezu 1.0) → Rückgabe x.
  if (q15 >= 32767) return x;
  if (q15 <= 0)     return 0;
  // 16x16 → 32 Bit, >>15, mit Sättigung
  int32_t y = (int32_t)x * q15;     // 16*15 -> 31 Bits
  y = (y + (1 << 14)) >> 15;        // Rundung
  if (y >  32767) return  32767;
  if (y < -32768) return -32768;
  return (int16_t)y;
}

// ------------------- Wiedergabe -------------------
static void play_from_ram() {
  if (!s_buf || s_bufSize <= WAV_HEADER_SIZE) return;
  if (!i2s_install_once()) return;

  s_playing.store(true, std::memory_order_release);

  const int16_t* mono = (const int16_t*)(s_buf + WAV_HEADER_SIZE);
  size_t monoSamples  = (s_bufSize - WAV_HEADER_SIZE) / 2;

  // aktuellen Vol-Faktor lesen (Mute hat Vorrang)
  int32_t vol_q15 = s_muted.load(std::memory_order_acquire) ? 0 : s_volQ15.load(std::memory_order_acquire);

  // Arbeits-Puffer: 512 Mono -> 1024 Stereo-Samples
  int16_t stereo[512 * 2];
  size_t index = 0;

  while (index < monoSamples) {
    size_t chunk = (monoSamples - index > 512) ? 512 : (monoSamples - index);

    // Mono -> Volume -> Stereo (L=R), mit Sättigung
    for (size_t i = 0; i < chunk; ++i) {
      int16_t s = mono[index + i];
      int16_t s_vol = scale_q15_sat(s, vol_q15);
      stereo[2*i + 0] = s_vol;  // L
      stereo[2*i + 1] = s_vol;  // R
    }

    size_t written = 0;
    esp_err_t err = i2s_write(I2S_NUM_0, stereo, chunk * 4, &written, portMAX_DELAY);
    if (err != ESP_OK) {
      Serial.printf("❌ i2s_write err=%d\n", (int)err);
      break;
    }
    index += chunk;
  }

  s_playing.store(false, std::memory_order_release);

  ensure_done_queue();
  if (s_doneQueue) {
    uint8_t ev = 1;
    xQueueSend(s_doneQueue, &ev, 0);
  }
}

// ------------------- Grid-Scheduler (kein Catch-Up) -------------------
static TickType_t next_aligned_beat_after(TickType_t now) {
  TickType_t period = (s_periodTicks == 0) ? 1 : s_periodTicks;
  TickType_t elapsed = now - s_epochTicks;
  TickType_t beatsPassed = elapsed / period;
  TickType_t nextBeat = s_epochTicks + (beatsPassed + 1) * period;
  if ((int32_t)(nextBeat - now) <= 0) nextBeat = now + 1;
  return nextBeat;
}

// ------------------- AudioTask (Core 1) -------------------
static void audio_task(void*) {
  i2s_install_once();
  ensure_mutex();
  ensure_done_queue();

  uint32_t period_ms = bpm_to_period_ms(s_bpm);
  s_periodTicks = ms_to_ticks(period_ms);
  s_epochTicks  = xTaskGetTickCount();

  Serial.printf("⏱️ AudioTask ready | BPM=%u | period=%u ms\n", (unsigned)s_bpm, (unsigned)period_ms);

  for (;;) {
    TickType_t nowTicks  = xTaskGetTickCount();
    TickType_t nextTicks = next_aligned_beat_after(nowTicks);
    TickType_t wait      = nextTicks - nowTicks;
    if (wait > 0) vTaskDelay(wait);

    s_beatCounter++;

#if AUDIO_TELEM_VERBOSE
    Serial.printf("beat #%llu | hold=%d playing=%d\n",
                  (unsigned long long)s_beatCounter,
                  (int)s_hold.load(), (int)s_playing.load());
#endif

    const uint32_t planned_ms = now_ms();
    bool startedOnThisBeat = false;

    if (!s_hold.load(std::memory_order_acquire) &&
        !s_playing.load(std::memory_order_acquire)) {

      xSemaphoreTake(s_bufMutex, portMAX_DELAY);
      bool ready = (s_buf != nullptr && s_bufSize > WAV_HEADER_SIZE);
      xSemaphoreGive(s_bufMutex);

      if (ready) {
        // Telemetrie: Drift & Überlänge
        const uint32_t t0_ms     = now_ms();                // Ist-Start
        const uint32_t wav_ms    = current_wav_duration_ms();
        const int32_t  drift_ms  = (int32_t)t0_ms - (int32_t)planned_ms;
        const uint32_t per_ms    = bpm_to_period_ms(s_bpm);
        const uint32_t skipped   = (wav_ms + per_ms - 1) / per_ms;
        const uint32_t miss_len  = (skipped > 0) ? (skipped - 1) : 0;

        s_avgDriftMs += (drift_ms - s_avgDriftMs) / DRIFT_AVG_WINDOW;
        s_startCounter++;
        s_lastStartMs   = t0_ms;
        s_lastPlannedMs = planned_ms;
        s_lastDriftMs   = drift_ms;
        if (miss_len > 0) {
          s_missedBeats_dueToLength_total += miss_len;
        }

#if AUDIO_TELEM
        Serial.printf(
          "🧭 Start #%llu | Soll=%u ms | Ist=%u ms | Drift=%d ms | Periode=%u ms | WAV=%u ms",
          (unsigned long long)s_startCounter,
          (unsigned)planned_ms, (unsigned)t0_ms, (int)drift_ms,
          (unsigned)per_ms, (unsigned)wav_ms
        );
        if (miss_len > 0) {
          Serial.printf(" | ⚠️ Überlänge: +%u Beat(s)", (unsigned)miss_len);
        }
        // globale Missed-Beats in dieser Ära (Tick-Sicht) + durch Länge (Start-Sicht)
        Serial.printf(" | ØDrift=%d ms | Missed(total)=%llu len=%llu | Vol=%u%%%s\n",
                      (int)s_avgDriftMs,
                      (unsigned long long)s_missedBeats_total,
                      (unsigned long long)s_missedBeats_dueToLength_total,
                      (unsigned)s_volumePercent.load(),
                      s_muted.load() ? " (MUTE)" : "");
#endif

        play_from_ram();
        startedOnThisBeat = true;
      }
    }

    if (!startedOnThisBeat) {
      s_missedBeats_total++;
      s_missedBeats_recent++;
#if AUDIO_TELEM_VERBOSE
      Serial.printf("⏱️ Missed beat #%llu (total=%llu, recent=%u)\n",
                    (unsigned long long)s_beatCounter,
                    (unsigned long long)s_missedBeats_total,
                    (unsigned)s_missedBeats_recent);
#endif
    }
  }
}

// ------------------- Public API -------------------
void audio_init() {
  ensure_mutex();
  ensure_done_queue();
  xTaskCreatePinnedToCore(audio_task, "AudioTask", 8192, nullptr, 2, &s_audioTask, 1);
}

void audio_set_bpm(uint16_t bpm) {
  if (bpm == 0) bpm = 1;
  s_bpm = bpm;

  uint32_t period_ms = bpm_to_period_ms(s_bpm);
  s_periodTicks = ms_to_ticks(period_ms);
  s_epochTicks  = xTaskGetTickCount();

  // Telemetrie Reset
  s_beatCounter = 0;
  s_startCounter = 0;
  s_missedBeats_total = 0;
  s_missedBeats_recent = 0;
  s_missedBeats_dueToLength_total = 0;
  s_avgDriftMs = 0;

  Serial.printf("⏱️ BPM=%u | period=%u ms\n", (unsigned)s_bpm, (unsigned)period_ms);
}

uint16_t audio_get_bpm()           { return s_bpm; }
uint32_t audio_get_period_ms()     { return bpm_to_period_ms(s_bpm); }

void audio_set_buffer(uint8_t* data, size_t size) {
  ensure_mutex();
  xSemaphoreTake(s_bufMutex, portMAX_DELAY);
  s_buf = data;
  s_bufSize = size;
  xSemaphoreGive(s_bufMutex);

  Serial.printf("📦 Buffer set: %u bytes\n", (unsigned)size);
}

bool audio_is_playing() { return s_playing.load(std::memory_order_acquire); }

bool audio_take_playback_done(uint32_t timeout_ms) {
  ensure_done_queue();
  if (!s_doneQueue) return false;
  uint8_t ev;
  return xQueueReceive(s_doneQueue, &ev, ms_to_ticks(timeout_ms)) == pdTRUE;
}

void audio_set_hold(bool hold) { s_hold.store(hold, std::memory_order_release); }
bool audio_get_hold()          { return s_hold.load(std::memory_order_acquire); }

// ------------------- Volume/Mute API (NEU) -------------------
void audio_set_volume(uint8_t percent) {
  if (percent > 100) percent = 100;
  s_volumePercent.store(percent, std::memory_order_release);
  // 0..100%  →  0..32767 (Q15)
  int32_t q15 = (int32_t)((uint32_t)percent * 32767u / 100u);
  if (q15 < 0) q15 = 0;
  if (q15 > 32767) q15 = 32767;
  s_volQ15.store(q15, std::memory_order_release);

#if AUDIO_TELEM
  Serial.printf("🔉 Volume: %u%% (%d/32767 Q15)%s\n",
                (unsigned)percent, (int)q15, s_muted.load() ? " [MUTED]" : "");
#endif
}

uint8_t audio_get_volume() {
  return s_volumePercent.load(std::memory_order_acquire);
}

void audio_mute(bool enable) {
  s_muted.store(enable, std::memory_order_release);
#if AUDIO_TELEM
  Serial.printf("🔇 Mute: %s\n", enable ? "ON" : "OFF");
#endif
}

bool audio_is_muted() {
  return s_muted.load(std::memory_order_acquire);
}