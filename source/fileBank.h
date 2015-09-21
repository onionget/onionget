#pragma once
#include "dataContainer.h"
#include "dll.h"

struct fileBankPrivate; 

typedef struct fileBankObject{
  struct fileBankPrivate *privateMembers; 
  dataContainerObject    *(*getFile)(struct fileBankObject *this, char *filename, uint32_t filenameBytesize);
}fileBankObject;


fileBankObject *newFileBank(void);