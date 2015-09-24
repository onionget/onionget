#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "routerBank.h"
#include "memoryManager.h"
#include "router.h"
#include "ogEnums.h"
#include "macros.h"
#include "bank.h"

typedef struct routerBankPrivate{
  routerBankObject   publicRouterBank; 
  bankObject          *routerObjects; 
}routerBankPrivate;

static int deposit(routerBankObject *this, routerObject *router);

routerBankObject *newRouterBank(void)
{
  routerBankPrivate *privateThis = NULL; 
  
  //allocate memory for object
  privateThis = (routerBankPrivate *)secureAllocate(sizeof(*privateThis));   
  if(privateThis == NULL){
    logEvent("Error", "Failed to allocate memory for router bank object");
    return NULL; 
  }
  
  //initialize public methods
  privateThis->publicRouterBank.depositRouterObject  = &depositRouterObject; 
  privateThis->publicRouterBank.getRouterObject      = &withdrawRouterObject;
  
  //initialize private properties TODO dependency inject this?
  privateThis->routerObjects = newBank(); 

  
  return (routerBankObject *)privateThis;
}

//returns 0 on error and 1 on success
static int deposit(routerBankObject *this, routerObject *router)
{
  routerBankPrivate *private = (routerBankPrivate *)this; 

  
  if(private == NULL || router == NULL || private->routerObjects == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0; 
  }
  
  
  if( !private->routerObjects->deposit(private->routerObjects, router) ){
    logEvent("Error", "Failed to generate a new router object");
    return 0; 
  }
  
  return 1; 
}

//TODO MUTEX HERE
//returns NULL on error or no available router objects, else returns a pointer to available router object
static routerObject *withdraw(routerBankObject *this)
{
  routerBankPrivate *private = (routerBankPrivate *)this;
  
  if(private == NULL || private->routerObjects == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return NULL; 
  }
  
  return (routerObject *) private->routerObjects->withdraw(private->routerObjects); 
}




