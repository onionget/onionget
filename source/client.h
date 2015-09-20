#pragma once

#include <stdint.h>
#include "router.h"
#include "dataContainer.h"


typedef struct clientObject{
  routerObject  *router;
  char          *torBindAddress;
  char          *torPort;
  char          *onionAddress;
  char          *onionPort;
  char          *dirPath; 
  char          *operation; 
  uint32_t      dirPathBytesize; 
  uint32_t      fileCount;
  char          **fileNames; 
  
  int          (*executeOperation)(struct clientObject *this); 
  int           (*getFiles)(struct clientObject *this);   
}clientObject; 

clientObject* newClient(char *torBindAddress, char *torPort, char *onionAddress, char *onionPort, char *operation, char *dirPath, char **fileNames, uint32_t fileCount);

