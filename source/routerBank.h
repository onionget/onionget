#pragma once
#include "router.h"
#include "dll.h"

typedef struct routerBankObject{
  int          (*depositRouterObject)(struct routerBankObject *this, routerObject *router)
  routerObject *(*withdrawRouterObject)(struct routerBankObject *this);
}routerBankObject;

routerBankObject *newRouterBank(void);