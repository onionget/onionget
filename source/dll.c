#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

#include "dataContainer.h"
#include "dll.h"
#include "memoryManager.h"
#include "ogEnums.h"



typedef struct dllMember
{
  struct dllMember   *previous;
  struct dllMember   *next;
  void               *memberData; 
}dllMember;


typedef struct dllPrivate{
  dllObject        publicDll; 
  struct dllMember *head;
  struct dllMember *tail;
  uint32_t         count; 
}dllPrivate;

//public methods
static int       insert(dllObject *this, int end, void *memberData);


//private methods
static int       insertInitial(dllObject *this, dllMember *member);
static int       insertHead(dllObject *this, dllMember *member);
static int       insertTail(dllObject *this, dllMember *member);
static dllMember *newDllMember(void *memberPointer);

/************ OBJECT CONSTRUCTOR ******************/

/*
 * newDll returns NULL on error, or a new doubly linked list object (with head and tail NULL) on success 
 */
dllObject* newDll(void)
{
  dllPrivate *privateThis = NULL;
  
  privateThis = (dllprivate *)secureAllocate(sizeof(*privateThis));
  if(privateThis == NULL){
    logEvent("Error", "Failed to allocate memory for linked list");
    return NULL; 
  }
  
  privateThis->head   = NULL; 
  privateThis->tail   = NULL;
  privateThis->count  = 0; 
  
  privateThis->publicDll.insert = &insert; 
  
  return (dllObject *)privateThis;
}


/************ PUBLIC METHODS ******************/


/*
 * insert returns 0 on error and 1 on success. Items may be inserted at end DLL_HEAD or DLL_TAIL, for the
 * start or end of the list, respectively. 
 */
static int insert(dllObject *this, int end, void *memberData)
{
  dllPrivate *private      = NULL 
  dllMember  *member       = NULL;
  int        insertSuccess = 0;
  
  //first some sanity checking
  if(this == NULL || memberData == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0; 
  }
  
  private = (dllPrivate *)this;

  if(end != DLL_HEAD && end != DLL_TAIL){
    logEvent("Error", "end must be DLL_HEAD or DLL_TAIL");
    return 0; 
  }
  
  //sanity checks passed so allocate a new member
  member = newDllMember(memberData);
  if(member == NULL){
    logEvent("Error", "Failed to create dll member");
    return 0;
  }


  //insert the item into the list according to the end specified 
  insertSuccess = 0; 
  
  if      ( private->head == NULL     )  insertSuccess = insertInitial ( this , member ); 
  else if ( end           == DLL_HEAD )  insertSuccess = insertHead    ( this , member );
  else if ( end           == DLL_TAIL )  insertSuccess = insertTail    ( this , member ); 
  
  if(!insertSuccess){
    secureFree(&member, sizeof(*member)); 
    logEvent("Error", "Failed to insert member into list");
    return 0; 
  }
  
  //keep track of the total bytesize of the linked list
  private->count += 1;
  
  return 1; 
}


/************ PRIVATE METHODS ******************/

/*
 * newDllObject returns NULL on error, or a pointer to a new dllObject on success. 
 */
static dllMember *newDllMember(void *memberData)
{ 
  dllMember *member;
  
  //allocate the dllObject 
  member = (dllMember *)secureAllocate(sizeof(*member));
  if(member == NULL){
    logEvent("Error", "Failed to allocate memory for linked list member");
    return NULL;
  }
  
  member->memberData    = memberData; 
  
  return member;
}

/*
 * insertInitial returns 0 on error and 1 on success. It's for inserting the initial item into a linked list. 
 */
static inline int insertInitial(dllObject *this, dllMember *member)
{
  privateDll *private = NULL;
  
  if(this == NULL || member == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  private = (dllPrivate *)this;
  
  private->head       = member;
  private->tail       = member;
  member->next        = NULL;
  member->previous    = NULL;    
  
  return 1;
}

/*
 * insertHead returns 0 on error and 1 on success. It inserts an item at the head of the list. 
 */
static inline int insertHead(dllObject *this, dllMember *member)
{
  privateDll *private = NULL;
  
  if(this == NULL || member == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  private = (dllPrivate *)this;
  
  private->head->previous = member;
  member->next            = private->head;
  member->previous        = NULL;
  private->head           = member;
  
  return 1;
}

/*
 * insertTail returns 0 on error and 1 on success. It's for inserting an item as the tail of the list. 
 */
static inline int insertTail(dllObject *this, dllMember *member)
{
  privateDll *private = NULL;
  
  if(this == NULL || member == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  private = (dllPrivate *)this;

  private->tail->next = member;
  member->previous    = private->tail;
  member->next        = NULL;
  private->tail       = member;
 
  return 1;
}