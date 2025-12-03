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

- Adafruit_SSD1306
- ESP32-audioI2S: https://github.com/schreibfaul1/ESP32-audioI2S/
- U8g2lib: https://github.com/olikraus/u8g2


## File system
- install LittleFS:


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


### Open Topis

## Hardware

- Serienwiderstand + C für Encoder (1kohm + 10nF?) oder 100nF + 10kOhm Pullup? + 470Serie?
- alle buttons gleiche logik (pulldown/active high)?
- LED an anderen Pin (aktuell BoardLED seriell; mit separatem ESP nicht mehr nötig?)

### Akku
- Lipo Akku. Beispiel: https://ydlbattery.com/products/3-7v-3000mah-804864-lithium-polymer-ion-battery
- Ladestrom checken / Wärmeentwicklung
- Verbrauch messen
- Akkustand anzeige: Bestenfalls Prozent, notfalls nur Battery low, ggf. Balken
- Display Dimmbar? / Display abschalten

### Esp32 / Board
- Welcher ESP 32, wie viel Flash?
- Eine USB-C Schnittstelle für Laden und Programmieren und Sample-Upload

### Samples
- Max Tempo vs Sample length, bei ca 240+ bpm some samples too long. Bei 240bpm 250ms max length.

### Enclosure
- https://www.bopla.de/gehaeusefinder
- https://www.fischerelektronik.de/web_fischer/en_GB/cases/M1/Cases/index.xhtml
- https://www.takachi-enclosure.com/
- https://www.takachi-enclosure.com/cat/extruded_aluminium_enclosures
- https://www.tme.eu/de/details/hm-1455k1202/gehause-mit-panel/hammond/
- https://www.okw.com/en/Aluminium-enclosures/Smart-Terminal.htm?ref=41d170d0-6960-11e5-b123-8eba63e66ed5
- http://toollessplasticenclosures.com/
- https://www.frontpanelexpress.com/
- https://www.schaeffer-ag.de/
- Beachte: Material, CE, EMV Prüfung (EMC) -> 

#### Inspiration
- https://www.reddit.com/r/diypedals/
- https://www.reddit.com/r/diypedals/comments/1olhobk/objekt_808_and_v7_built_for_a_customer/
- https://teenage.engineering/store