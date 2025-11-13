#include "audio.h"
#include "driver/i2s.h"
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// ------------------- Debug -------------------
// #define AUDIO_DEBUG_BEAT 1

// ------------------- Konfiguration -------------------
constexpr uint32_t SAMPLE_RATE     = 44100; // ggf. an deine WAVs anpassen
constexpr size_t   WAV_HEADER_SIZE = 44;    // Standard-WAV-Header

// ------------------- Zustände -------------------
static TaskHandle_t       s_audioTask    = nullptr;
static SemaphoreHandle_t  s_bufMutex     = nullptr;
static std::atomic<bool>  s_playing{false};
static float              s_bpm          = 60.0f;

static uint8_t* s_buf     = nullptr;  // aktueller WAV-Buffer (nur lesend in AudioTask)
static size_t   s_bufSize = 0;

// Beat-/Zeitsteuerung (starrer Grid anhand einer Epoch)
static TickType_t s_periodTicks  = 0;   // Ticks pro Beat
static TickType_t s_epochTicks   = 0;   // Start-Epoch für das Grid (wird bei init/BPM-Change gesetzt)

// I2S-Installations-Flag
static bool s_i2sInstalled = false;

// Playback-Done Event-Queue
static QueueHandle_t s_doneQueue = nullptr;   // 1-Byte Events; 1 = playback finished

// ------------------- Hilfen -------------------
static inline void ensure_mutex() {
  if (!s_bufMutex) {
    s_bufMutex = xSemaphoreCreateMutex();
    if (s_bufMutex) Serial.println("🔒 audio: mutex created");
  }
}

static inline void ensure_done_queue() {
  if (!s_doneQueue) {
    s_doneQueue = xQueueCreate(4, sizeof(uint8_t));
    if (s_doneQueue) Serial.println("📮 audio: done-queue created");
  }
}

static inline TickType_t ms_to_ticks(uint32_t ms) {
  TickType_t t = pdMS_TO_TICKS(ms);
  return (t == 0) ? 1 : t;
}

// ------------------- I2S Setup (einmalig) -------------------
static bool i2s_install_once() {
  if (s_i2sInstalled) return true;

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;    // Stereo-Bus (L/R)
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

// ------------------- Wiedergabe aus RAM -------------------
static void play_from_ram() {
  if (!s_buf || s_bufSize <= WAV_HEADER_SIZE) return;
  if (!i2s_install_once()) return;

  s_playing.store(true, std::memory_order_release);

  const int16_t* mono = (const int16_t*)(s_buf + WAV_HEADER_SIZE);
  size_t monoSamples = (s_bufSize - WAV_HEADER_SIZE) / 2; // 2 Bytes pro 16-bit-Sample

  // Arbeits-Puffer: 512 Mono -> 1024 Stereo-Samples (L=R)
  int16_t stereo[512 * 2];
  size_t index = 0;

  while (index < monoSamples) {
    size_t chunk = (monoSamples - index > 512) ? 512 : (monoSamples - index);
    for (size_t i = 0; i < chunk; ++i) {
      int16_t s = mono[index + i];
      stereo[2 * i + 0] = s;
      stereo[2 * i + 1] = s;
    }

    size_t written = 0;
    // 4 Bytes pro Stereo-Frame (2 * 16-bit)
    esp_err_t err = i2s_write(I2S_NUM_0, stereo, chunk * 4, &written, portMAX_DELAY);
    if (err != ESP_OK) {
      Serial.printf("❌ i2s_write err=%d\n", (int)err);
      break;
    }
    index += chunk;
  }

  s_playing.store(false, std::memory_order_release);

  // Playback-Done Event senden (nicht blockierend)
  ensure_done_queue();
  if (s_doneQueue) {
    uint8_t ev = 1;
    xQueueSend(s_doneQueue, &ev, 0);
  }
}

// ------------------- Beat-Scheduler (kein Catch-Up) -------------------
static TickType_t next_aligned_beat_after(TickType_t now) {
  // Nächster Beat >= now, ohne Aufholen
  // k = ceil((now - epoch) / period)
  TickType_t period = (s_periodTicks == 0) ? 1 : s_periodTicks;

  // Achtung auf Wrap-around (TickType_t ist uint32_t): subtraction ist okay.
  TickType_t elapsed = now - s_epochTicks;
  TickType_t beatsPassed = elapsed / period;
  TickType_t nextBeat = s_epochTicks + (beatsPassed + 1) * period;

  // Sollte nextBeat == now sein, eine minimale Tick-Wartezeit einfügen
  if ((int32_t)(nextBeat - now) <= 0) {
    nextBeat = now + 1;
  }
  return nextBeat;
}

// ------------------- AudioTask (Core 1) -------------------
static void audio_task(void*) {
  i2s_install_once();
  ensure_mutex();
  ensure_done_queue();

  // Beat-Parameter festlegen und Epoch setzen
  s_periodTicks = ms_to_ticks((uint32_t)(60000.0f / (s_bpm < 1.0f ? 1.0f : s_bpm)));
  s_epochTicks  = xTaskGetTickCount(); // ab jetzt läuft der Grid

  Serial.printf("⏱️ AudioTask ready | BPM=%.2f | period=%u ms\n", s_bpm, (unsigned)(60000.0f / s_bpm));

  for (;;) {
    // Auf nächsten in-der-Zukunft liegenden Beat warten (kein Catch-Up)
    TickType_t now  = xTaskGetTickCount();
    TickType_t next = next_aligned_beat_after(now);
    TickType_t wait = next - now;
    if (wait > 0) vTaskDelay(wait);

    #if AUDIO_DEBUG_BEAT
    Serial.println("beat");
    #endif

    // Start NUR auf Beat, NUR wenn nicht bereits eine Wiedergabe läuft, und NUR wenn ein Buffer gesetzt ist
    if (!s_playing.load(std::memory_order_acquire)) {
      xSemaphoreTake(s_bufMutex, portMAX_DELAY);
      bool ready = (s_buf != nullptr && s_bufSize > WAV_HEADER_SIZE);
      xSemaphoreGive(s_bufMutex);

      if (ready) {
        play_from_ram(); // blockiert ausschließlich diese Task (Core 1)
        // Nach Ende: nächste Iteration richtet sich wieder am Grid aus (ceil)
      }
    }
  }
}

// ------------------- Public API -------------------
void audio_init() {
  ensure_mutex();
  ensure_done_queue();
  // Task auf Core 1 pinnen
  xTaskCreatePinnedToCore(audio_task, "AudioTask", 8192, nullptr, 2, &s_audioTask, 1);
}

void audio_set_bpm(float bpm) {
  if (bpm < 1.0f) bpm = 1.0f;
  s_bpm = bpm;

  // Periode neu setzen und Grid neu an "jetzt" anheften
  s_periodTicks = ms_to_ticks((uint32_t)(60000.0f / s_bpm));
  s_epochTicks  = xTaskGetTickCount(); // neuer Grid ab jetzt

  Serial.printf("⏱️ BPM=%.2f | period=%u ms\n", s_bpm, (unsigned)(60000.0f / s_bpm));
}

void audio_set_buffer(uint8_t* data, size_t size) {
  ensure_mutex();
  xSemaphoreTake(s_bufMutex, portMAX_DELAY);
  s_buf = data;
  s_bufSize = size;
  xSemaphoreGive(s_bufMutex);

  Serial.printf("📦 Buffer set: %u bytes\n", (unsigned)size);
}

bool audio_is_playing() {
  return s_playing.load(std::memory_order_acquire);
}

bool audio_take_playback_done(uint32_t timeout_ms) {
  ensure_done_queue();
  if (!s_doneQueue) return false;
  uint8_t ev;
  return xQueueReceive(s_doneQueue, &ev, ms_to_ticks(timeout_ms)) == pdTRUE;
}