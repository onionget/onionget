#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "datacontainer.h"
#include "memorymanager.h"

static int destroyDataContainer(dataContainer **thisPointer);

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

//returns 0 on error
static int destroyDataContainer(dataContainer **thisPointer)
{
  dataContainer  *this;
  
  this = *thisPointer;   
  
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  if( this->data != NULL ){
    if(!secureFree( &(this->data), this->bytesize ) ){
      printf("Error: Failed to free dataContainer data\n");
      return 0;      
    }
  }
  
  if( !secureFree(thisPointer, sizeof(struct dataContainer)) ){
    printf("Error: Failed to free data container object\n");
    return 0; 
  }
  
  
  return 1; 
}