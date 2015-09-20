#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "memoryManager.h"
#include "connection.h"
#include "server.h"
#include "router.h"

static int destroyConnectionObject(connectionObject **thisPointer);

/*
 * Keeps trying to allocate an connectionObject object, if fails due to out of memory, sleeps one second and tries again forever
 */
connectionObject *newActiveConnection(serverObject *server) //TODO consider adding timeout period or something, currently this can block forever, though
{                                                           //typically this will be desired behavior...  
  connectionObject *connection;
  
  if(server == NULL){
    printf("Error: Something was NULL that shouldn't have beena\n");
    return NULL; 
  }
  
  do{
    connection = (connectionObject *)secureAllocate(sizeof(*connection));
    if(connection == NULL){
      sleep(1);
      continue; 
    }
 
    connection->connectedRouter = newRouter(); 
    if(connection->connectedRouter == NULL){
      secureFree(&connection, sizeof(*connection)); 
      sleep(1);
      continue;
    }
    
    connection->server                  = server;
    connection->destroyConnectionObject = &destroyConnectionObject; 
    break; 
  }while(1);
    
  return connection; 
}

static int destroyConnectionObject(connectionObject **thisPointer)
{
  connectionObject  *this; 
  
  this = *thisPointer; 
  
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }
  
  if(this->connectedRouter != NULL && !this->connectedRouter->destroyRouter(&this->connectedRouter)){
    printf("Error: Failed to destroy active connection router");
    return 0; 
  }
  
  if(!secureFree(thisPointer, sizeof(struct connectionObject)) ){
    printf("Error: Failed to free the active connection object\n");
    return 0;
  }
  
  return 1; 
}