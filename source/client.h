#pragma once

#include <stdint.h>
#include "router.h"


typedef struct clientObject{  
  int          (*getFiles)(struct clientObject *this, char *dirPath, char **fileNames, uint32_t fileCount);   
  int          (*establishConnection)(struct clientObject *this, char *onionAddress, char *onionPort);
  int          (*setRouter)(struct clientObject *client, routerObject *router);  
  int          (*initializeSocks)(struct clientObject *client, char *torBindAddress, char *torPort);
}clientObject; 


clientObject *newClient(void);

