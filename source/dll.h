#pragma once
#include <stdint.h>
#include "dataContainer.h"


typedef struct dllMember
{
  struct dllMember   *previous;
  struct dllMember   *next;
  void               *memberData; 
  volatile int       locked; 
}dllMember;

typedef struct dllObject{
  struct dllMember *head;
  struct dllMember *tail;
  
  uint32_t count; 
  
  int (*insert)(struct dllObject *this, int end, void *memberData);  
}dllObject;


dllObject *newDll(void);

