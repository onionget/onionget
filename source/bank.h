#pragma once
#include <stdint.h>


typedef struct bankObject{  
  int (*deposit)(struct bankObject *this, void *pointer);
  void *(*withdraw)(struct bankObject this);
}bankObject;


bankObject *newBank(void);

