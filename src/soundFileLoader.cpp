#include "SoundFileLoader.h"

bool initializeSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to initialize SPIFFS");
        return false;
    }
    return true;
}

int loadSoundFiles(const char *soundFiles[], int maxFiles) {
    int fileIndex = 0;

    File root = SPIFFS.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("Failed to open root directory");
        return 0;
    }

    File file = root.openNextFile();
    while (file && fileIndex < maxFiles) {
        if (!file.isDirectory()) {
            String fileName = file.name();
            if (fileName.endsWith(".wav")) {
                soundFiles[fileIndex] = strdup(fileName.c_str()); // Copy file name
                Serial.printf("Added: %s\n", soundFiles[fileIndex]);
                fileIndex++;
            }
        }
        file = root.openNextFile();
    }

    Serial.printf("Total .wav files loaded: %d\n", fileIndex);
    return fileIndex; // Return the number of files loaded
}
