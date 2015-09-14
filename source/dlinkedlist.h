#pragma once
#include <stdint.h>
#include "datacontainer.h"


typedef struct dllObject
{
  struct dllObject *previous;
  struct dllObject *next;
  char             *identifier; 
  dataContainer    *dataContainer;
}dllObject;

typedef struct dll{
  struct dllObject *head;
  struct dllObject *tail;
  uint64_t         bytesize;
  
  dataContainer *(*getId)(struct dll* this, char* id, uint64_t idBytesize); 
  int          (*insert)(struct dll* this, int end, char* id, uint64_t idBytesize, dataContainer* dataContainer);  
}dll;


dll *newDll(void);

