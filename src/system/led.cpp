// led.cpp
#include "led.h"
#include "pins.h"
#include "config.h"

namespace LEDTask {

    // Optional 'volatile' – int-Zugriffe sind i.d.R. atomar, aber volatile dokumentiert nebenläufigen Zugriff
    static volatile int bpm = BPM_START;
    static volatile bool enabled = true;
    static TaskHandle_t handle = nullptr;

    // µs -> RTOS-Ticks, immer mind. 1 Tick warten
    static inline TickType_t usToMinTicks(int64_t us) {
        if (us <= 0) return 1;
        TickType_t t = pdMS_TO_TICKS((us + 999) / 1000); // auf nächste ms runden
        return (t == 0) ? 1 : t;
    }

    void setBPM(int newBpm) {
        bpm = constrain(newBpm, BPM_MIN, BPM_MAX);
    }

    int getBPM() { return bpm; }

    void setEnabled(bool on) { enabled = on; }
    bool isEnabled() { return enabled; }

    static void ledTask(void*) {
        // pinMode(Pins::LED, OUTPUT);   // <-- Entfernt: Pin-Setup erfolgt zentral in main.cpp

        // deterministischer Start: erster Beat in einer vollen Periode
        int localBPM   = bpm;
        int64_t period = 60000000LL / localBPM;           // µs pro Beat
        int64_t nextBeat = esp_timer_get_time() + period; // erster Beat

        for (;;) {
            // BPM-Schnappschuss & Periodenberechnung
            localBPM = bpm;
            period   = 60000000LL / localBPM;

            // bis zum nächsten Beat schlafen (nicht blockierend)
            int64_t now = esp_timer_get_time();
            int64_t wait = nextBeat - now;
            if (wait > 0) {
                vTaskDelay(usToMinTicks(wait));
            } else {
                // Verzug: Beat neu terminieren
                nextBeat = esp_timer_get_time() + period;
            }


            // LED nur schalten, wenn enabled == true
            if (enabled) {
                digitalWrite(Pins::LED, HIGH);
                vTaskDelay(pdMS_TO_TICKS(LED_ON_TIME_MS));
                digitalWrite(Pins::LED, LOW);
            }


            // nächsten Beat vorbereiten
            nextBeat += period;
        }
    }

    void start() {
        if (!handle) {
            xTaskCreatePinnedToCore(
                ledTask,
                "LED",
                2048,
                nullptr,
                1,      // niedrige Priorität reicht aus
                &handle,
                0       // Core 0 (gemeinsam mit Encoder/GUI/Control)
            );
        }
    }
}