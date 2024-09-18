# Metronome

This repository documents the creation of a metronome with the ESP32.

## Setup

Use PlatformIO IDE extension for VSCODE to run this project: https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide

Select Board: DOIT ESP32 Devkit V1

### Libraries

- Button2
- Adafruit_GFX
- Adafruit_SSD1306
- ESP32Encoder: https://github.com/madhephaestus/ESP32Encoder.git
- ESP32-audioI2S: https://github.com/schreibfaul1/ESP32-audioI2S/
- U8g2lib: https://github.com/olikraus/u8g2

#### ESP32-audioI2S

nur 16bit, 10kHz ok
500Hz Fullscale Wav max. audio.setVolume(7)\
besser 6

## File system

> ### Deprecated implementation:
>
> - Only works with 1.8.x
> - LittleFS installieren:
> - https://randomnerdtutorials.com/esp32-littlefs-arduino-ide/
> - https://randomnerdtutorials.com/install-esp32-filesystem-uploader-arduino-ide/#macos-installing

### Upload WAV files

Based on this Tutorial hochladen: https://randomnerdtutorials.com/arduino-ide-2-install-esp32-littlefs/

Arduino IDE --> Tools --> ESP32 Sketch Data Upload --> SPIFFS

Log files stored in filessystem: https://techtutorialsx.com/2019/02/23/esp32-arduino-list-all-files-in-the-spiffs-file-system/

## Hardware

### Amp:

100nF + 1000µF Kapazität vor Vin Amp
100K zwischen Vin und Gain
SD auf 3V3

### Circut

A circuit schematic can be found here: [Cuircuit diagram](circuit.pdf)

> The diagram currently still has mistakes.

## User Interface

Library used U8g2:

- Wiki: https://github.com/olikraus/u8g2/wiki/u8g2setupcpp
- Constructor reference: https://github.com/olikraus/u8g2/wiki/u8g2setupcpp#ssd1305-128x32_noname

### Create UI assets:

Lopaka Workflow: https://www.youtube.com/watch?v=Eyvzw_ujcS0

1. Go to https://lopaka.app/
2. Make sure library is set to U8g2 and resolution to 128x64px

## Issues

- **Sketch uses 116% of available space:** \
  Can also have this error message: "Text section exceeds available space in board"\
  Can be fixed by choosing a partition scheme **minimal SPIFFS (Large Apps with OTA)** in the menu: _Tools / Partition scheme_
- **Menu _Tools / Partition scheme_ not showing in Arduino IDE 2.x**:\
  Can be fixed by modifying board.txt: https://forum.arduino.cc/t/partition-menu-not-showing-esp32/1229446/4

> Dreprecated:
> **Arduino IDE Setup**
> Board: DOIT ESP32 Devkit V1 bis (Needs to be added manually. See issues below)
> https://randomnerdtutorials.com/installing-esp32-arduino-ide-2-0/
