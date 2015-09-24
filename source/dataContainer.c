#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "dataContainer.h"
#include "memoryManager.h"
#include "macros.h"

static int destroyDataContainer(dataContainerObject **thisPointer);


/************ OBJECT CONSTRUCTOR ******************/

/*           
 *
 * returns NULL on error, on success returns a dataContainer object
 * 
 * dataContainer objects are simply structs for data buffers allocated for bytesize bytes,
 * the data pointer is of type unsigned char*, pointing to up to 2^64 bytes 
 *
 */
dataContainerObject *newDataContainer(size_t bytesize)
{
  dataContainerObject *this;
  
  //allocate memory for data container
  this = (dataContainerObject*)secureAllocate(sizeof(*this));
  if(this == NULL){
    logEvent("Error", "Failed to allocate memory for data container");
    return NULL; 
  }
  
  //set properties
  this->data = (char*) secureAllocate(bytesize);
  if(this->data == NULL){
    logEvent("Error", "Failed to allocate memory for data");
    secureFree(&this, sizeof(*this));
    return NULL; 
  }
  
  this->bytesize = bytesize;
  
  //set public methods
  this->destroyDataContainer  = &destroyDataContainer;

  return this;
}

/************ PUBLIC METHODS ******************/

/*           
 * returns 0 on error and 1 on success. Simply frees the memory associated with the dataContainer (data buffer and object)
 */
static int destroyDataContainer(dataContainerObject **thisPointer)
{
  dataContainerObject *this;
  
  this = *thisPointer;   
  
  if(this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  //if the data pointer isn't pointed to NULL then securely free it
  if( this->data != NULL ){
    if(!secureFree( &(this->data), this->bytesize ) ){
      logEvent("Error", "Failed to free dataContainer data");
      return 0;      
    }
  }
  
  //securely free the object memory
  if( !secureFree(thisPointer, sizeof(**thisPointer)) ){
    logEvent("Error", "Failed to free data container object");
    return 0; 
  }
  
  
  return 1; 
}