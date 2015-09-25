#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>


#include "connection.h"
#include "router.h"
#include "ogEnums.h"
#include "macros.h"


static int reinitialize(connectionObject *this);

connectionObject *newConnection(void)
{
  connectionObject *this = (connectionObject *)secureAllocate(sizeof(*this));
  if( this == NULL ){
    logEvent("Error", "Failed to allocate connection object");
    return NULL; 
  }
  
  this->router            = newRouter();
  this->requestedFilename = (char *)secureAllocate(MAX_FILE_ID_BYTESIZE);
  this->dataCache         = NULL; //mmap
  
  this->reinitialize      = &reinitialize; 
 
  return this; 
}


static int reinitialize(connectionObject *this)
{
  if( this == NULL || this->router == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0; 
  }
  
  if( !this->router->reinitialize(this->router) ){
    logEvent("Error", "Failed to reinitialize connection.");
    return 0; 
  }
  
  if( !memoryClear(this->requestedFilename, MAX_FILE_ID_BYTESIZE) ){
    logEvent("Error", "Failed to reinitialize connection.");
    return 0; 
  }
  
  if( munmap(this->dataCache, FILE_CHUNK_BYTESIZE) ){ //TODO think more about this, make sure it can be done, or do this differently, needs more thought into this
    logEvent("Error", "Failed to unmap memory read");
    return 0; 
  }
  
  return 1; 
}