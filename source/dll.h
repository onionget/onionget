#pragma once
#include <stdint.h>
#include "dataContainer.h"


typedef struct dllMember
{
  struct dllMember       *previous;
  struct dllMember       *next;
  char                   *identifier; 
  size_t                 identifierBytesize; 
  dataContainerObject    *dataContainer;
}dllMember;

typedef struct dllObject{
  struct dllMember *head;
  struct dllMember *tail;
  
  uint32_t memberCount; 
  
  dataContainerObject *(*getId)(struct dllObject *this, char *id, uint32_t idBytesize); 
  int                  (*insert)(struct dllObject *this, int end, char *id, size_t idBytesize, dataContainerObject *dataContainer);  
}dllObject;


dllObject *newDll(void);

