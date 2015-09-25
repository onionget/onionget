#pragma once
#include "connection.h"
#include "dll.h"

typedef struct connectionBankObject{
  int              (*deposit)(struct connectionBankObject *this, connectionObject *connection)
  connectionObject *(*withdraw)(struct connectionBankObject *this);
}connectionBankObject;

connectionBankObject *newConnectionBank(uint32_t slots);