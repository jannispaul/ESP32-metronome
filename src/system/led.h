#pragma once
#include <Arduino.h>

namespace LEDTask {

    // Initialisiert LED (PinMode etc.) und startet Task
    void start();

    // Setzt den aktuellen BPM-Wert (wird z. B. aus controlTask übergeben)
    void setBPM(int bpm);

    // Optional für spätere Synchronisation mit Audio/GUI
    int getBPM();


    void setEnabled(bool on);  // LED sichtbar ja/nein
    bool isEnabled();

}
