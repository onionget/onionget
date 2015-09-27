#pragma once

typedef struct serverObject{
  int (*beginOperation)(const char *sharedFolderPath, uint32_t maxCacheBytesize, char *bindAddress, char *listenPort); 
}serverObject;


serverObject *newServer(void);