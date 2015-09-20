#pragma once
#include "stdint.h"
#include "dataContainer.h"

typedef struct diskFileObject{
  char     *mode;
  char     *fullPath;
  uint32_t fullPathBytesize; 
  FILE     *descriptor; 
  
  uint32_t (*diskFileWrite)(struct diskFileObject* this, dataContainerObject* dataContainer, uint32_t writeOffset);
  dataContainerObject *(*diskFileRead)(struct diskFileObject* this, uint32_t bytesToRead, uint32_t readOffset); 
  int (*closeTearDown)(struct diskFileObject** thisPointer); 
  long int (*getBytesize)(struct diskFileObject* this);
}diskFileObject; 


diskFileObject* newDiskFile(char* path, char* name, char* mode); 
