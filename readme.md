# Metronome

This repository documents the creation of a metronome with the ESP32.

## Table of Contents

0. [Concept](#concept)
1. [Setup](#setup)
2. [File system](#file-system)
3. [Hardware](#hardware)
4. [User Interface](#user-interface)
5. [Issues](#issues)

## Concept

To our knowledge there is not good metronome available on the market. This project is an attempt to create a metronome with a user interface that is easy to use and has a couple of additional features.
Inspiration and concept board: https://www.figma.com/design/YPMkHb1VGQY4GaYAEU80sy/Metronome?node-id=102-12&node-type=frame&t=6EPdNIxbqS0YYaIc-11

## Setup

This project is **not** intended for use with Arduino IDE but with **PlatformIO** instead.
Use [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) to run this project.

In PlatformIO: -> Select Board: DOIT ESP32 Devkit V1

### Libraries

The project uses the following libraries. A list of the dependencies can also be found the config file [platformio.ini](platformio.ini)

- Button2
- Adafruit_GFX
- Adafruit_SSD1306
- ESP32Encoder: https://github.com/madhephaestus/ESP32Encoder.git
- ESP32-audioI2S: https://github.com/schreibfaul1/ESP32-audioI2S/
- U8g2lib: https://github.com/olikraus/u8g2

#### ESP32-audioI2S

- only use 16bit, 10kHz is ok
- 500Hz Fullscale Wav max. audio.setVolume(7), better 6

## File system

> ### Deprecated implementation:
>
> - Only works with 1.8.x
> - install LittleFS:
> - https://randomnerdtutorials.com/esp32-littlefs-arduino-ide/
> - https://randomnerdtutorials.com/install-esp32-filesystem-uploader-arduino-ide/#macos-installing

### Upload WAV files

Based on this Tutorial hochladen: https://randomnerdtutorials.com/arduino-ide-2-install-esp32-littlefs/
Arduino IDE --> Tools --> ESP32 Sketch Data Upload --> SPIFFS
Log files stored in filessystem: https://techtutorialsx.com/2019/02/23/esp32-arduino-list-all-files-in-the-spiffs-file-system/

## Hardware

### Amp:

- 100nF + 1000µF capacity before Vin Amp?
- 100K between Vin and Gain?
- SD to 3V3?

### Schematic

A circuit schematic can be found here: [schematic](schematic.pdf)

> The schematic is still under development.

## User Interface

Library used U8g2:

- Wiki: https://github.com/olikraus/u8g2/wiki/u8g2setupcpp
- Constructor reference: https://github.com/olikraus/u8g2/wiki/u8g2setupcpp#ssd1305-128x32_noname

### Create UI assets:

Lopaka Workflow inspired by: https://www.youtube.com/watch?v=Eyvzw_ujcS0

1. Create UI in Figma
2. Export PNGS
3. Go to https://lopaka.app/
4. Make sure library is set to U8g2 and resolution to 128x64px
5. Import pngs, add them to screen and check "Declare as PROGMEM" in code settings
6. Copy bitmap code to project

## Issues & Troubleshooting

- **Only gibberish in the serial monitor?** Make sure the baud rate is set to 115200. Set `monitor_speed = 115200` in platformio.ini.
- **Sketch uses 116% of available space:** \
  Can also have this error message: "Text section exceeds available space in board"\
  Can be fixed by choosing a partition scheme **minimal SPIFFS (Large Apps with OTA)** in the menu: _Tools / Partition scheme_
- **Menu _Tools / Partition scheme_ not showing in Arduino IDE 2.x**:\
  Can be fixed by modifying board.txt: https://forum.arduino.cc/t/partition-menu-not-showing-esp32/1229446/4

> Dreprecated:
> **Arduino IDE Setup**
> Board: DOIT ESP32 Devkit V1 bis (Needs to be added manually. See issues below)
> https://randomnerdtutorials.com/installing-esp32-arduino-ide-2-0/
