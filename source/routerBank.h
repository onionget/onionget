#pragma once
#include "router.h"
#include "dll.h"

typedef struct routerBankObject{
  routerObject *(*getRouterObject)(struct routerBankObject *this);
}routerBankObject;

routerBankObject *newRouterBank(void);