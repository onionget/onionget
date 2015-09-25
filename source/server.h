#pragma once
#include "router.h"
#include "dll.h"

typedef struct serverObject{
  int (*processConnections)(struct serverObject *this); 
}serverObject;


serverObject *newServer(void);