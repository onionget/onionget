#pragma once
#include "router.h"
#include "dlinkedlist.h"

typedef struct server{
  char     *sharedFolderPath;
  int      sharedFolderPathBytesize;
  char     *bindAddress;
  int      listenPort;
  uint64_t maxMemoryCacheBytesize; 
  dll      *cachedSharedFiles;  
  router  *listeningRouter; 
  
  int (*serverListen)(struct server* this); 
}server;

typedef struct activeConnection{
  server *server;
  router *connectedRouter; 
}activeConnection; 





server *newServer(char *sharedFolderPath, char *bindAddress, int listenPort, uint64_t maxMemoryCacheMegabytes);