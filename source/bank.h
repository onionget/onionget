#pragma once
#include <stdint.h>


typedef struct bankObject{  
  int (*deposit)(struct bankObject *this, void *pointer, char *id, uint32_t idBytesize);
  void *(*withdraw)(struct bankObject this);
  void *(*getPointerById)(struct bankObject *this, char *id, uint32_t idBytesize);
}bankObject;


bankObject *newBank(void);

