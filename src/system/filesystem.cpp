#include "filesystem.h"
#include <vector>  // 🔧 ebenfalls hier nötig

bool fs_init() {
    if (!LittleFS.begin()) {
        Serial.println("⚠️  LittleFS konnte nicht gemountet werden!");
        return false;
    }
    Serial.println("✅ LittleFS bereit.");
    return true;
}

void fs_print_info() {
    size_t total = LittleFS.totalBytes();
    size_t used  = LittleFS.usedBytes();
    Serial.printf("LittleFS Info:\n  Gesamt: %u Bytes\n  Belegt: %u Bytes\n  Frei:   %u Bytes\n\n",
                  total, used, total - used);
}

void fs_list_files(const char *dirname, uint8_t levels) {
    File root = LittleFS.open(dirname);
    if (!root || !root.isDirectory()) {
        Serial.println("❌ Verzeichnis konnte nicht geöffnet werden oder ist ungültig.");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.printf("[DIR]  %s\n", file.name());
            if (levels) fs_list_files(file.name(), levels - 1);
        } else {
            String name = file.name();
            if (!name.startsWith("/")) name = "/" + name;
            Serial.printf("[FILE] %s (%u Bytes)\n", name.c_str(), file.size());
        }
        file = root.openNextFile();
    }
}

std::vector<String> fs_get_all_wav_files(const char *dirname) {
    std::vector<String> files;
    File root = LittleFS.open(dirname);
    if (!root || !root.isDirectory()) {
        Serial.println("❌ Verzeichnis konnte nicht geöffnet werden oder ist ungültig.");
        return files;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = file.name();
            if (!name.startsWith("/")) name = "/" + name;
            if (name.endsWith(".wav") || name.endsWith(".WAV")) {
                files.push_back(name);
            }
        }
        file = root.openNextFile();
    }
    return files;
}

uint8_t* fs_load_file(const char *path, size_t &size) {
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.printf("❌ Datei %s konnte nicht geöffnet werden.\n", path);
        return nullptr;
    }

    size = file.size();
    uint8_t *buffer = (uint8_t *)malloc(size);
    if (!buffer) {
        Serial.println("❌ Nicht genug RAM!");
        file.close();
        return nullptr;
    }

    file.read(buffer, size);
    file.close();
    Serial.printf("📦 %s in RAM geladen (%u Bytes)\n", path, size);
    return buffer;
}
