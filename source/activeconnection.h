#pragma once
#include "server.h"
#include "router.h"

typedef struct activeConnection{
  server *server;
  router *connectedRouter; 
  int (*destroyActiveConnection)(struct activeConnection **thisPointer);
}activeConnection; 

activeConnection *newActiveConnection(server *server); //TODO stop using type name as var name! 