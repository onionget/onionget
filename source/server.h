#pragma once
#include "router.h"

typedef struct server{
  char     *sharedFolderPath;
  int      sharedFolderPathBytesize;
  char     *bindAddress;
  int      listenPort;
  uint64_t maxMemoryCache; 
  dll      *cachedSharedFiles;  
  router  *listeningRouter; 
  
  void (*serverListen)(struct server* this); 
}server;

typedef struct activeConnection{
  server *server;
  router *connectedRouter; 
}activeConnection; 





server* newServer(int argc, char *argv[]);