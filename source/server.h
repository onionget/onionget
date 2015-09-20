#pragma once
#include "router.h"
#include "dll.h"

typedef struct serverObject{
  char           *sharedFolderPath;
  int            sharedFolderPathBytesize;
  char           *bindAddress;
  int            listenPort;
  uint32_t       maxMemoryCacheBytesize; 
  dllObject      *cachedSharedFiles;  
  routerObject   *listeningRouter; 
  
  int (*serverListen)(struct serverObject *this); 
}serverObject;







serverObject *newServer(char *sharedFolderPath, char *bindAddress, int listenPort, uint16_t maxMemoryCacheMegabytes);