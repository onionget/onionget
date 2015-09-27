#pragma once
#include "stdint.h"


typedef struct diskFileObject{
  uint32_t            (*dfWrite)(struct diskFileObject* this, void *dataBuffer, size_t bytesize, uint32_t writeOffset);
  int                 (*dfRead)(struct diskFileObject *this, void* outBuffer, uint32_t bytesToRead, uint32_t readOffset);
  int                 (*closeTearDown)(struct diskFileObject** thisPointer); 
  int                 (*dfOpen)(struct diskFileObject *this, const char *path, char *name, char *mode);
  uint32_t            (*getBytesize)(struct diskFileObject *this);
  char                *(*getFilename)(struct diskFileObject *this); 
  uint32_t            (*cacheBytes)(struct diskFileObject *this, uint32_t maxBytes); 
}diskFileObject; 


diskFileObject* newDiskFile(void); 



