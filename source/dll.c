#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "dataContainer.h"
#include "dll.h"
#include "memoryManager.h"
#include "ogEnums.h"


//public methods
static int                   insert(dllObject *this, int end, char *id, size_t idBytesize, dataContainerObject *dataContainer);
static dataContainerObject   *getId(dllObject *this, char *id, uint32_t idBytesize);

//private methods
static int       insertInitial(dllObject *this, dllMember *member);
static int       insertHead(dllObject *this, dllMember *member);
static int       insertTail(dllObject *this, dllMember *member);
static dllMember *newDllMember(dataContainerObject *dataContainer, char *id, size_t idBytesize);

/************ OBJECT CONSTRUCTOR ******************/

/*
 * newDll returns NULL on error, or a new doubly linked list object (with head and tail NULL) on success 
 */
dllObject* newDll(void)
{
  dllObject *this;
  
  this = (dllObject *)secureAllocate(sizeof(*this));
  if(this == NULL){
    printf("Error: Failed to allocate memory for linked list\n");
    return NULL; 
  }
  
  this->head     = NULL; 
  this->tail     = NULL;
  
  this->memberCount = 0; 
  
  this->insert  = &insert;
  this->getId   = &getId;
  
  return this;
}


/************ PUBLIC METHODS ******************/

/*
 *  getId returns NULL if an error occurs or the item identified by id is not found in the list. On success it returns a pointer to to requested dataContainer.
 */
static dataContainerObject *getId(dllObject *this, char *id, uint32_t idBytesize)
{
  dllMember *currentMember;
  
  if(this == NULL || id == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL; 
  }
  
  for(currentMember = this->head; currentMember != NULL; currentMember = currentMember->next){
    if( !memcmp(currentMember->identifier, id, idBytesize) ){
      return currentMember->dataContainer;
    }
  }
   
  return NULL; 
}

/*
 * insert returns 0 on error and 1 on success. It inserts to the linked list dataContainer object dataContainer
 * identified by id id, which is idBytesize bytes. Items may be inserted at end DLL_HEAD or DLL_TAIL, for the
 * start or end of the list, respectively. 
 */
static int insert(dllObject *this, int end, char *id, size_t idBytesize, dataContainerObject *dataContainer)
{
  dllMember  *member;
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
  
  //sanity checks passed so allocate a new member
  member = newDllMember(dataContainer, id, idBytesize);
  if(member == NULL){
    printf("Error: Failed to create dll member\n");
    return 0;
  }


  //insert the item into the list according to the end specified 
  insertSuccess = 0; 
  
  if      ( this->head == NULL     )  insertSuccess = insertInitial ( this , member ); 
  else if ( end        == DLL_HEAD )  insertSuccess = insertHead    ( this , member );
  else if ( end        == DLL_TAIL )  insertSuccess = insertTail    ( this , member ); 
  
  if(!insertSuccess){
    secureFree(&(member->identifier), member->identifierBytesize);
    secureFree(&member, sizeof(*member)); 
    printf("Error: Failed to insert member into list\n");
    return 0; 
  }
  
  //keep track of the total bytesize of the linked list
  this->memberCount += 1;
  
  return 1; 
}


/************ PRIVATE METHODS ******************/

/*
 * newDllObject returns NULL on error, or a pointer to a new dllObject on success. 
 */
static dllMember *newDllMember(dataContainerObject *dataContainer, char *id, size_t idBytesize)
{ 
  dllMember  *member;
  
  if(dataContainer == NULL || id == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL;
  }
  
  //allocate the dllObject 
  member = (dllMember *)secureAllocate(sizeof(*member));
  if(member == NULL){
    printf("Error: Failed to allocate memory for linked list member\n");
    return NULL;
  }
  
  //member is identified by identifier id, which is idBytesize bytes
  member->identifier = (char *)secureAllocate(idBytesize);
  if(member->identifier == NULL){
    printf("Error: Failed to allocate memory for linked list member identified\n");
    secureFree(&member, sizeof(*member));
    return NULL; 
  }
  
  member->identifierBytesize = idBytesize; 
  
  //write the id into the member identifier field
  memcpy(member->identifier, id, idBytesize); 
  
  //add the dataContainer to the dllObject
  member->dataContainer = dataContainer; 
  
  return member;
}

/*
 * insertInitial returns 0 on error and 1 on success. It's for inserting the initial item into a linked list. 
 */
static inline int insertInitial(dllObject *this, dllMember *member)
{
  if(this == NULL || member == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  this->head       = member;
  this->tail       = member;
  member->next     = NULL;
  member->previous = NULL;    
  
  return 1;
}

/*
 * insertHead returns 0 on error and 1 on success. It inserts an item at the head of the list. 
 */
static inline int insertHead(dllObject *this, dllMember *member)
{
  if(this == NULL || member == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  this->head->previous = member;
  member->next         = this->head;
  member->previous     = NULL;
  this->head           = member;
  
  return 1;
}

/*
 * insertTail returns 0 on error and 1 on success. It's for inserting an item as the tail of the list. 
 */
static inline int insertTail(dllObject *this, dllMember *member)
{
  if(this == NULL || member == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  

  this->tail->next = member;
  member->previous = this->tail;
  member->next     = NULL;
  this->tail       = member;
 
  return 1;
}