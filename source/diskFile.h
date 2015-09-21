#pragma once
#include "stdint.h"
#include "dataContainer.h"

typedef struct diskFileObject{
  uint32_t            (*dfWrite)(struct diskFileObject* this, dataContainerObject* dataContainer, uint32_t writeOffset);
  dataContainerObject *(*dfRead)(struct diskFileObject* this, uint32_t bytesToRead, uint32_t readOffset); 
  int                 (*closeTearDown)(struct diskFileObject** thisPointer); 
  long int            (*dfBytesize)(struct diskFileObject* this);
  int                 (*dfOpen)(struct diskFileObject *this, char *path, char *name, char *mode);
}diskFileObject; 


diskFileObject* newDiskFile(void); 
