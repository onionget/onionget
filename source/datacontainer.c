#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "datacontainer.h"
#include "memorymanager.h"

static void destroyDataContainer(dataContainer** thisPointer);

//returns null on error
dataContainer* newDataContainer(uint64_t bytesize)
{
  dataContainer *this;
  
  //allocate memory for data container
  this = secureAllocate(sizeof(struct dataContainer));
  if(this == NULL){
    printf("Error: Failed to allocate memory for data container\n");
    return NULL; 
  }
  
  //set properties
  this->data = secureAllocate(bytesize);
  if(this->data == NULL){
    printf("Error: Failed to allocate memory for data\n");
    secureFree(&this, sizeof(struct dataContainer));
    return NULL; 
  }
  
  this->bytesize = bytesize;
  
  //set public methods
  this->destroyDataContainer  = &destroyDataContainer;

  return this;
}


static void destroyDataContainer(dataContainer** thisPointer)
{
  dataContainer  *this;
  
  this = *thisPointer;   
  
  secureFree( &(this->data), this->bytesize );
  secureFree(thisPointer, sizeof(struct dataContainer));
}