#ifndef SOUND_FILE_LOADER_H
#define SOUND_FILE_LOADER_H

#include "FS.h"
#include "SPIFFS.h"

#define MAX_SOUND_FILES 10 // Adjust as needed

// Function to initialize SPIFFS
bool initializeSPIFFS();

// Function to load sound files into the array
int loadSoundFiles(const char *soundFiles[], int maxFiles);

#endif // SOUND_FILE_LOADER_H
