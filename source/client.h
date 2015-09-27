#pragma once

#include <stdint.h>
#include "router.h"
#include "diskFile.h"


typedef struct clientObject{  
  int          (*getFiles)(struct clientObject *this, char *dirPath, char **fileNames, uint32_t fileCount, diskFileObject *clientFileInterface);   
  int          (*establishConnection)(struct clientObject *this, char *onionAddress, char *onionPort);
  int          (*initializeSocks)(struct clientObject *client, char *torBindAddress, char *torPort);
}clientObject; 


clientObject *newClient(routerObject *router);

