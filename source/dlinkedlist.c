#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "datacontainer.h"
#include "dlinkedlist.h"
#include "memorymanager.h"


//public methods
static int insert(dll  *this, int end, char *id, uint64_t idBytesize, dataContainer *dataContainer);
static dataContainer   *getId(dll *this, char *id, uint64_t idBytesize);

//private methods
static int       insertInitial(dll *this, dllObject *object);
static int       insertHead(dll *this, dllObject *object);
static int       insertTail(dll *this, dllObject *object);
static dllObject *newDllObject(dataContainer *dataContainer, char *id, uint64_t idBytesize);

/************ OBJECT CONSTRUCTOR ******************/

/*
 * newDll returns NULL on error, or a new doubly linked list object (with head and tail NULL) on success 
 */
dll* newDll(void)
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


/************ PUBLIC METHODS ******************/

/*
 *  getId returns NULL if an error occurs or the item identified by id is not found in the list. On success it returns a pointer to to requested dataContainer.
 */
static dataContainer *getId(dll *this, char *id, uint64_t idBytesize)
{
  dllObject *currentObject;
  
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

/*
 * insert returns 0 on error and 1 on success. It inserts to the linked list dataContainer object dataContainer
 * identified by id id, which is idBytesize bytes. Items may be inserted at end DLL_HEAD or DLL_TAIL, for the
 * start or end of the list, respectively. 
 */
static int insert(dll *this, int end, char *id, uint64_t idBytesize, dataContainer *dataContainer)
{
  dllObject  *object;
  int        insertSuccess;
  
  //first some sanity checking
  if(this == NULL || id == NULL || dataContainer == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }

  if(end != DLL_HEAD && end != DLL_TAIL){
    printf("Error: end must be DLL_HEAD or DLL_TAIL\n");
    return 0; 
  }
  
  //sanity checks passed so allocate a new object
  object = newDllObject(dataContainer, id, idBytesize);


  //insert the item into the list according to the end specified 
  insertSuccess = 0; 
  
  if      ( this->head == NULL     )  insertSuccess = insertInitial ( this , object ); 
  else if ( end        == DLL_HEAD )  insertSuccess = insertHead    ( this , object );
  else if ( end        == DLL_TAIL )  insertSuccess = insertTail    ( this , object ); 
  
  if(!insertSuccess){
    printf("Error: Failed to insert object into list\n");
    return 0; 
  }
  
  //keep track of the total bytesize of the linked list
  this->bytesize += dataContainer->bytesize;
  
  return 1; 
}


/************ PRIVATE METHODS ******************/

static dllObject *newDllObject(dataContainer *dataContainer, char *id, uint64_t idBytesize)
{ 
  
  dllObject  *object;
  
  if(dataContainer == NULL || id == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL;
  }
  
  //allocate the dllObject 
  object = secureAllocate(sizeof(struct dllObject));
  if(object == NULL){
    printf("Error: Failed to allocate memory for linked list object\n");
    return NULL;
  }
  
  //object is identified by identifier id, which is idBytesize bytes
  object->identifier = secureAllocate(idBytesize);
  if(object->identifier == NULL){
    printf("Error: Failed to allocate memory for linked list object identified\n");
    return NULL; //add free
  }
  
  //write the id into the object identifier field
  memcpy(object->identifier, id, idBytesize); 
  
  //add the dataContainer to the dllObject
  object->dataContainer = dataContainer; 
  
  return object;
}

/*
 * insertInitial returns 0 on error and 1 on success. It's for inserting the initial item into a linked list. 
 */
static inline int insertInitial(dll *this, dllObject *object)
{
  if(this == NULL || object == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  this->head       = object;
  this->tail       = object;
  object->next     = NULL;
  object->previous = NULL;    
  
  return 1;
}

/*
 * insertHead returns 0 on error and 1 on success. It inserts an item at the head of the list. 
 */
static inline int insertHead(dll *this, dllObject *object)
{
  if(this == NULL || object == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  this->head->previous = object;
  object->next         = this->head;
  object->previous     = NULL;
  this->head           = object;
  
  return 1;
}

/*
 * insertTails returns 0 on error and 1 on success. It's for inserting an item as the tail of the list. 
 */
static inline int insertTail(dll *this, dllObject *object)
{
  if(this == NULL || object == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  

  this->tail->next = object;
  object->previous = this->tail;
  object->next     = NULL;
  this->tail       = object;
 
  return 1;
}