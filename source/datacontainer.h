#pragma once
#include <stdint.h>

typedef struct dataContainer{
  unsigned char* data;
  uint64_t bytesize;
  
  void (*destroyDataContainer)(struct dataContainer** thisPointer);
}dataContainer;

dataContainer* newDataContainer(uint64_t bytesize);