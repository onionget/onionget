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
  
  int          (*executeOperation)(struct client* this, int argc, char *argv[]); 
  int           (*getFiles)(struct client* this, int argc, char *argv[]);   
}client; 

client* newClient(int argc, char* argv[]);