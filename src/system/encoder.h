// system/encoder.h
#pragma once
#include <Arduino.h>

namespace Encoder {
  extern volatile int clicks;   // existiert bereits

  // Liefert die seit dem letzten Aufruf aufgelaufenen Klicks und setzt sie auf 0
  int consumeClicks();

  void init();
}