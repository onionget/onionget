#pragma once
#include <stdint.h>
#include "dataContainer.h"




typedef struct dllObject{  
  int (*insert)(struct dllObject *this, int end, void *memberData);  
}dllObject;


dllObject *newDll(void);

