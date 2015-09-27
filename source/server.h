#pragma once
#include "router.h"

typedef struct serverObject{
  int (*serve)(const char *sharedFolderPath, uint32_t maxCacheBytesize, char *bindAddress, char *listenPort); 
}serverObject;


serverObject *newServer(routerObject *router, diskFileObject** fileBank, connectionObject** connectionBank);

