#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>


#include "connectionBank.h"
#include "memoryManager.h"
#include "ogEnums.h"
#include "macros.h"
#include "bank.h"

typedef struct connectionBankPrivate{
  connectionBankObject   publicConnectionBank; 
  bankObject             *connectionObjects; 
}connectionBankPrivate;

static int deposit(connectionBankObject *this, connectionObject *connection);

static pthread_mutex_t depositLock  = PTHREAD_MUTEX_INITIALIZER; 
static pthread_mutex_t withdrawLock = PTHREAD_MUTEX_INITIALIZER;


connectionBankObject *newConnectionBank(uint32_t slots)
{
  connectionBankPrivate *privateThis = NULL; 
  
  if(slots == 0){
    return NULL; 
  }
  
  //allocate memory for object
  privateThis = (connectionBankPrivate *)secureAllocate(sizeof(*privateThis));   
  if(privateThis == NULL){
    logEvent("Error", "Failed to allocate memory for connection bank object");
    return NULL; 
  }
  
  //initialize public methods
  privateThis->publicConnectionBank.deposit  = &deposit; 
  privateThis->publicConnectionBank.withdraw = &withdraw;
  
  //initialize private properties TODO dependency inject this?
  privateThis->connectionObjects = newBank(slots); 
  
  
  return (connectionBankObject *)privateThis;
}

         
//returns 0 on error and 1 on success
static int deposit(connectionBankObject *this, connectionObject *connection)
{
  pthread_mutex_lock(&depositLock); 

  connectionBankPrivate *private = (connectionBankPrivate *)this; 

  if(private == NULL || connection == NULL || private->connectionObjects == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    pthread_mutex_unlock(&depositLock); 
    return 0; 
  }
  
  
  if( !private->connectionObjects->deposit(private->connectionObjects, connection) ){
    logEvent("Error", "Failed to deposit a connection object");
    pthread_mutex_unlock(&depositLock); 
    return 0; 
  }
  
  pthread_mutex_unlock(&depositLock); 
  
  return 1; 
}


//returns NULL on error or no available connection objects, else returns a pointer to available connection object
static connectionObject *withdraw(connectionBankObject *this)
{
  pthread_mutex_lock(&withdrawLock);
  
  connectionBankPrivate *private = (connectionBankPrivate *)this;
  
  if(private == NULL || private->connectionObjects == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    pthread_mutex_unlock(&withdrawLock); 
    return NULL; 
  }
  
  pthread_mutex_unlock(&withdrawLock); 
  
  return (connectionObject *) private->connectionObjects->withdraw(private->connectionObjects); 
}




