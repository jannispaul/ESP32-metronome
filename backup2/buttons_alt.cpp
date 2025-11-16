#include "buttons.h"
#include "pins.h"

namespace Buttons {

    static bool last1 = LOW;
    static bool last2 = LOW;
    static bool last3 = LOW;

    void init() {
        pinMode(Pins::Button1, INPUT);  // Buttons -> 3V3 → kein Pullup!
        pinMode(Pins::Button2, INPUT);
        pinMode(Pins::Button3, INPUT);
    }

    void task(void* param) {
        unsigned long lastChange = 0;
        for (;;) {
            bool b1 = digitalRead(Pins::Button1);
            bool b2 = digitalRead(Pins::Button2);
            bool b3 = digitalRead(Pins::Button3);

            // einfache Entprellung (30 ms)
            unsigned long now = millis();
            if (now - lastChange > 30) {
                if (b1 != last1) {
                    last1 = b1;
                    lastChange = now;
                    if (b1 == HIGH) Serial.println("Button1 pressed");
                }
                if (b2 != last2) {
                    last2 = b2;
                    lastChange = now;
                    if (b2 == HIGH) Serial.println("Button2 pressed");
                }
                if (b3 != last3) {
                    last3 = b3;
                    lastChange = now;
                    if (b3 == HIGH) Serial.println("Button3 pressed");
                }
            }

            vTaskDelay(10 / portTICK_PERIOD_MS);  // non-blocking
        }
    }
}
