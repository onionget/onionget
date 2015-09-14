#pragma once

#include <stdint.h>
#include "router.h"
#include "datacontainer.h"


typedef struct client{
  router        *router;
  dataContainer *fileRequestString;
  char          *torBindAddress;
  char          *torPort;
  char          *onionAddress;
  char          *onionPort;
  char          *dirPath; 
  char          *operation; 
  uint32_t      dirPathBytesize; 
  uint32_t      fileCount;
  char          **fileNames; 
  
  int          (*executeOperation)(struct client* this); 
  int           (*getFiles)(struct client* this);   
}client; 

client* newClient(char *torBindAddress, char *torPort, char *onionAddress, char *onionPort, char *operation, char *dirPath, char **fileNames, uint32_t fileCount);