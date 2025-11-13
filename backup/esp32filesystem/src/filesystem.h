#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <vector>     // 🔧 WICHTIG: für std::vector

// Initialisiert LittleFS
bool fs_init();

// Gibt Infos über belegten/freien Speicher aus
void fs_print_info();

// Listet Dateien auf
void fs_list_files(const char *dirname = "/", uint8_t levels = 1);

// Liest alle WAV-Dateien im Verzeichnis ein (Namen)
std::vector<String> fs_get_all_wav_files(const char *dirname = "/");

// Lädt eine Datei vollständig in den RAM
uint8_t* fs_load_file(const char *path, size_t &size);
