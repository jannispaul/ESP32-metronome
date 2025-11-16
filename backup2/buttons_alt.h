#pragma once
#include <Arduino.h>

namespace Buttons {

    // Initialisierung aller Button-Pins
    void init();

    // Non-blocking FreeRTOS-Task zur Erkennung
    void task(void* param);

    // Später evtl. Callback-Funktionen oder EventFlags
}
