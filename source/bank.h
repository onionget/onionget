#pragma once
#include <stdint.h>


typedef struct bankObject{  
  int (*spawnSlot)(struct bankObject *this);
  int (*deposit)(struct bankObject *this, void *pointer);
  void *(*withdraw)(struct bankObject this);
}bankObject;


bankObject *newBank(void);

