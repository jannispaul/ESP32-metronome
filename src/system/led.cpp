
// led.cpp (neu)
#include "led.h"
#include "pins.h"
#include "config.h"
#include "audio.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace LEDTask {
    static volatile bool enabled = true;
    static TaskHandle_t handle = nullptr;
    static QueueHandle_t beatQ = nullptr;

    void setEnabled(bool on) { enabled = on; }
    bool isEnabled() { return enabled; }

    // Optional: BPM-API beibehalten für spätere Fallbacks
    static volatile int bpm = 120;
    void setBPM(int newBpm) { bpm = (newBpm <= 0) ? 1 : newBpm; }
    int  getBPM() { return bpm; }

    static void ledTask(void*) {
        pinMode(Pins::LED, OUTPUT);
        digitalWrite(Pins::LED, LOW);

        // Beat-Queue vom Audio-Modul besorgen
        beatQ = audio_get_beat_queue();

        // Falls Queue (noch) nicht existiert, periodisch erneut probieren
        while (!beatQ) {
            vTaskDelay(pdMS_TO_TICKS(50));
            beatQ = audio_get_beat_queue();
        }

        uint8_t ev;
        for (;;) {
            // Wartet effizient auf das nächste Beat-Event (kein Busy-Wait)
            if (xQueueReceive(beatQ, &ev, portMAX_DELAY) == pdTRUE) {
                if (enabled) {
                    vTaskDelay(pdMS_TO_TICKS(LED_OFFSET_MS)); //Offset für Audio-Synchronität
                    digitalWrite(Pins::LED, HIGH);
                    vTaskDelay(pdMS_TO_TICKS(LED_ON_TIME_MS));
                    digitalWrite(Pins::LED, LOW);
                }
                // Wenn disabled, Event einfach „verbraucht“ → kein Blink
            }
        }
    }

    void start() {
        if (!handle) {
            xTaskCreatePinnedToCore(
                ledTask,
                "LED",
                2048,
                nullptr,
                1,     // niedrige Priorität
                &handle,
                0      // Core 0 (zusammen mit GUI/Encoder/Buttons)
            );
        }
    }
}
