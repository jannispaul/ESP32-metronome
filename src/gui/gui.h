//gui.h
#pragma once
#include <Arduino.h>


namespace Gui {
  enum class Focus : uint8_t { BPM = 0, FILE = 1, VOL = 2 };

  struct Event {
    enum Type : uint8_t {
      BPM_CHANGED,
      VOL_CHANGED,
      FILE_CHANGED,
      FOCUS_CHANGED,
      MUTE_CHANGED
    } type;
    int value; // BPM, Vol, File Index, Focus (cast), Mute (0/1)
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


    void setFocus(Focus f);
    Focus getFocus();
    void nextFocus();

    void postVolume(int percent);
    void postFile(const String& name, int index, int count);

    void showInitialVolume(int percent);


} // namespace Gui
