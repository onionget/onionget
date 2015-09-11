#pragma once
#include "stdint.h"
#include "datacontainer.h"

typedef struct diskFile{
  char     *mode;
  char     *fullPath;
  uint32_t fullPathBytesize; 
  FILE     *descriptor; 
  
  int (*diskFileWrite)(struct diskFile* this, dataContainer* dataContainer);
  dataContainer *(*diskFileRead)(struct diskFile* this); 
  void (*closeTearDown)(struct diskFile** thisPointer); 
  long int (*getBytesize)(struct diskFile* this);
}diskFile; 


diskFile* newDiskFile(char* path, char* name, char* mode); 