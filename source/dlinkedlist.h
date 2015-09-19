#pragma once
#include <stdint.h>
#include "datacontainer.h"


typedef struct dllObject
{
  struct dllObject *previous;
  struct dllObject *next;
  char             *identifier; 
  size_t           identifierBytesize; 
  dataContainer    *dataContainer;
}dllObject;

typedef struct dll{
  struct dllObject *head;
  struct dllObject *tail;
  size_t           bytesize;
  
  dataContainer *(*getId)(struct dll* this, char* id, size_t idBytesize); 
  int          (*insert)(struct dll* this, int end, char* id, size_t idBytesize, dataContainer* dataContainer);  
}dll;


dll *newDll(void);

