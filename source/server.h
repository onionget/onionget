#pragma once
#include "router.h"
#include "dlinkedlist.h"

typedef struct server{
  char     *sharedFolderPath;
  int      sharedFolderPathBytesize;
  char     *bindAddress;
  int      listenPort;
  uint32_t maxMemoryCacheBytesize; 
  dll      *cachedSharedFiles;  
  router   *listeningRouter; 
  
  int (*serverListen)(struct server* this); 
}server;







server *newServer(char *sharedFolderPath, char *bindAddress, int listenPort, uint16_t maxMemoryCacheMegabytes);