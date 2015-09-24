#pragma once
#include <stdint.h>


typedef struct dllObject{  
  int (*spawnSlot)(struct dllObject *this);
  int (*depositPointer)(struct dllObject *this, void *pointer);
  void *(*withdrawPointer)(struct dllObject this);
}dllObject;


dllObject *newDll(void);

