#pragma once
#include "connection.h"
#include "dll.h"

typedef struct connectionBankObject{
  dllObject *connectionObjects;
  connectionObject *(*getConnectionObject)(struct connectionBankObject *this);
}connectionBankObject;

