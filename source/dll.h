#pragma once
#include <stdint.h>
#include "dataContainer.h"


typedef struct dllMember
{
  struct dllMember   *previous;
  struct dllMember   *next;
  void               *memberPointer; 
  uint8_t            locked; 
}dllMember;

typedef struct dllObject{
  struct dllMember *head;
  struct dllMember *tail;
  
  uint32_t count; 
  
  int (*insert)(struct dllObject *this, int end, char *id, size_t idBytesize, dataContainerObject *dataContainer);  
}dllObject;


dllObject *newDll(void);

