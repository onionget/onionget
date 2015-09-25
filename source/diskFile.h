#pragma once
#include "stdint.h"
#include "dataContainer.h"

typedef struct diskFileObject{
  uint32_t            (*dfWrite)(struct diskFileObject* this, void *dataBuffer, size_t bytesize, uint32_t writeOffset);
  dataContainerObject *(*dfRead)(struct diskFileObject* this, uint32_t bytesToRead, uint32_t readOffset); 
  int                 (*closeTearDown)(struct diskFileObject** thisPointer); 
  int                 (*dfOpen)(struct diskFileObject *this, char *path, char *name, char *mode);
  uint32_t            (*getBytesize)(diskFile *this);
}diskFileObject; 


diskFileObject* newDiskFile(void); 
