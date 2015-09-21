#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "routerBank.h"
#include "memoryManager.h"
#include "router.h"
#include "ogEnums.h"

typedef struct routerBankPrivate{
  routerBankObject   publicRouterBank; 
  dllObject          *routerObjects; 
}routerBankPrivate;


routerBankObject *newRouterBank(void)
{
  routerBankPrivate *privateThis = NULL; 
  
  //allocate memory for object
  privateThis = (routerBankPrivate *)secureAllocate(sizeof(*privateThis));   
  if(privateThis == NULL){
    printf("Error: Failed to allocate memory for router bank object\n");
    return NULL; 
  }
  
  //initialize public methods
  privateThis->publicRouterBank.depositRouterObject  = &depositRouterObject; 
  privateThis->publicRouterBank.getRouterObject      = &getRouterObject;
  
  //initialize private properties TODO dependency inject this?
  privateThis->routerObjects = newDll(); 

  
  return (routerBankObject *)privateThis;
}

//returns 0 on error and 1 on success
static int depositRouterObject(routerBankObject *this, routerObject *router)
{
  routerBankPrivate *private  = NULL; 
  
  private = (routerBankPrivate *)this; 
  
  if(private == NULL || this == NULL || router == NULL || private->routerObjects == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }
  
  
  if( !private->routerObjects->insert(private->routerObjects, DLL_TAIL, router) ){
    printf("Error: Failed to generate a new router object\n");
    return 0; 
  }
  
  return 1; 
}

