#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>


#include "routerBank.h"
#include "memoryManager.h"
#include "router.h"
#include "ogEnums.h"
#include "macros.h"
#include "bank.h"

typedef struct routerBankPrivate{
  routerBankObject   publicRouterBank; 
  bankObject         *routerObjects; 
}routerBankPrivate;

static int deposit(routerBankObject *this, routerObject *router);

static pthread_mutex_t depositLock  = PTHREAD_MUTEX_INITIALIZER; 
static pthread_mutex_t withdrawLock = PTHREAD_MUTEX_INITIALIZER;


routerBankObject *newRouterBank(uint32_t slots)
{
  routerBankPrivate *privateThis = NULL; 
  
  if(slots == 0){
    return NULL; 
  }
  
  //allocate memory for object
  privateThis = (routerBankPrivate *)secureAllocate(sizeof(*privateThis));   
  if(privateThis == NULL){
    logEvent("Error", "Failed to allocate memory for router bank object");
    return NULL; 
  }
  
  //initialize public methods
  privateThis->publicRouterBank.deposit  = &deposit; 
  privateThis->publicRouterBank.withdraw = &withdraw;
  
  //initialize private properties TODO dependency inject this?
  privateThis->routerObjects = newBank(slots); 
  
  
  return (routerBankObject *)privateThis;
}

         
//returns 0 on error and 1 on success
static int deposit(routerBankObject *this, routerObject *router)
{
  pthread_mutex_lock(&depositLock); 

  routerBankPrivate *private = (routerBankPrivate *)this; 

  if(private == NULL || router == NULL || private->routerObjects == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    pthread_mutex_unlock(&depositLock); 
    return 0; 
  }
  
  
  if( !private->routerObjects->deposit(private->routerObjects, router) ){
    logEvent("Error", "Failed to generate a new router object");
    pthread_mutex_unlock(&depositLock); 
    return 0; 
  }
  
  pthread_mutex_unlock(&depositLock); 
  
  return 1; 
}


//returns NULL on error or no available router objects, else returns a pointer to available router object
static routerObject *withdraw(routerBankObject *this)
{
  pthread_mutex_lock(&withdrawLock);
  
  routerBankPrivate *private = (routerBankPrivate *)this;
  
  if(private == NULL || private->routerObjects == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    pthread_mutex_unlock(&withdrawLock); 
    return NULL; 
  }
  
  pthread_mutex_unlock(&withdrawLock); 
  
  return (routerObject *) private->routerObjects->withdraw(private->routerObjects); 
}




