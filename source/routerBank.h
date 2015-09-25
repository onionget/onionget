#pragma once
#include "router.h"
#include "dll.h"

typedef struct routerBankObject{
  int          (*deposit)(struct routerBankObject *this, routerObject *router)
  routerObject *(*withdraw)(struct routerBankObject *this);
}routerBankObject;

routerBankObject *newRouterBank(uint32_t slots);