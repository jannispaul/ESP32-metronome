//gui.h
#pragma once
#include <Arduino.h>

namespace Gui {

    struct Event {
        enum Type { BPM_CHANGED } type;
        int value;
    };

    // GUI initialisieren (z. B. Display, Buffer, Fonts …)
    bool init();
    
    void showInitialBPM(int bpm);
    
    // BPM-Wert an GUI schicken (threadsafe über Queue)
    bool postBPM(int bpm);

    // Startet die GUI-Task (FreeRTOS, eigener Core)
    void startTask();

    void setMuted(bool muted);
    bool isMuted();

} // namespace Gui
