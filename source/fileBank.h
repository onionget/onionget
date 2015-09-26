#pragma once

#include <unistd.h>

#include "diskFile.h"


typedef struct fileBankObject{
  diskFileObject *(*getPointerById)(struct fileBankObject *this, char *requestedId, uint32_t idBytesize);
  int            (*deposit)(struct fileBankObject *this, diskFileObject *file);
}fileBankObject;

fileBankObject *newFileBank(uint32_t slots);

