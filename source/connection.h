#pragma once
#include "server.h"
#include "router.h"

typedef struct connectionObject{
  serverObject *server;
  routerObject *connectedRouter; 
  int (*destroyConnectionObject)(struct connectionObject **thisPointer);
}connectionObject; 

connectionObject *newActiveConnection(serverObject *server); 