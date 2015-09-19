#pragma once
#include <stdint.h>

typedef struct dataContainer{
  char* data;
  size_t bytesize;
  
  int (*destroyDataContainer)(struct dataContainer** thisPointer);
}dataContainer;

dataContainer* newDataContainer(size_t bytesize);