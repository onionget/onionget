#pragma once
#include <stdint.h>

typedef struct dataContainerObject{
  char   *data;
  size_t bytesize;
  
  int (*destroyDataContainer)(struct dataContainerObject** thisPointer);
}dataContainerObject;

dataContainerObject *newDataContainer(size_t bytesize);