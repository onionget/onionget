#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "datacontainer.h"
#include "dlinkedlist.h"
#include "memorymanager.h"



static int insert(dll* this, int end, char* id, uint64_t idBytesize, dataContainer* dataContainer);
dataContainer* getId(dll* this, char* id, uint64_t idBytesize);


dll* newDll()
{
  dll *this;
  
  this = secureAllocate(sizeof(struct dll));
  if(this == NULL){
    printf("Error: Failed to allocate memory for linked list\n");
    return NULL; 
  }
  
  this->head     = NULL; 
  this->tail     = NULL;
  this->bytesize = 0;
  
  this->insert  = &insert;
  this->getId   = &getId;
  
  return this;
}

//returns NULL on error or item not in list
dataContainer* getId(dll* this, char* id, uint64_t idBytesize)
{
  dllObject* currentObject;
  
  if(this == NULL || id == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL; 
  }
  
  for(currentObject = this->head; currentObject != NULL; currentObject = currentObject->next){
    if( !memcmp(currentObject->identifier, id, idBytesize) ){
      return currentObject->dataContainer;
    }
  }
   
  return NULL; 
}

//returns 0 on error
static int insert(dll* this, int end, char* id, uint64_t idBytesize, dataContainer* dataContainer)
{
  dllObject *object;
  
  if(this == NULL || id == NULL || dataContainer == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }

  if(end != DLL_HEAD && end != DLL_TAIL){
    printf("Error: end must be DLL_HEAD or DLL_TAIL\n");
    return 0; 
  }
  
  object = secureAllocate(sizeof(dllObject));
  if(object == NULL){
    printf("Error: Failed to allocate memory for linked list object\n");
    return 0;
  }
  
  object->identifier = secureAllocate(idBytesize);
  if(object->identifier == NULL){
    printf("Error: Failed to allocate memory for linked list object identified\n");
    return 0;
  }
  
  object->dataContainer = dataContainer; 

  memcpy(object->identifier, id, idBytesize); 
  
  this->bytesize += dataContainer->bytesize;
  
  if(this->head == NULL){
    this->head       = object;
    this->tail       = object;
    object->next     = NULL;
    object->previous = NULL;    
    return 1;
  }
  else if(end == DLL_HEAD){
    this->head->previous = object;
    object->next         = this->head;
    object->previous     = NULL;
    this->head           = object;
    return 1;
  }
  else if(end == DLL_TAIL){
    this->tail->next = object;
    object->previous = this->tail;
    object->next     = NULL;
    this->tail       = object;
    return 1;
  }
  else{
    printf("Error: Should never make it here!\n");
    return 0; 
  }
}