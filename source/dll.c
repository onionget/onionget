#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

#include "dataContainer.h"
#include "dll.h"
#include "memoryManager.h"
#include "ogEnums.h"


typedef struct dllSlot
{
  struct dllSlot   *previous;
  struct dllSlot   *next;
  void             *pointer; 
}dllSlot;


typedef struct dllPrivate{
  dllObject        publicDll; 
  struct dllSlot   *head;
  struct dllSlot   *tail;
  uint32_t         count; //deal with overflow potential ! TODO
}dllPrivate;


//public methods
static int        spawnSlot(dllObject *this);
static void       *withdrawPointer(dllObject *this);
static int        depositPointer(dllObject *this, void *pointer);

//private methods 
static inline int insertInitial(dllObject *this, dllSlot *slot);
static inline int insertHead(dllObject *this, dllSlot *slot);


/************ OBJECT CONSTRUCTOR ******************/


/*
 * newDll returns NULL on error, or a new doubly linked list object (with head and tail NULL) on success 
 */
dllObject* newDll(void)
{
  dllPrivate *privateThis = NULL;
  
  //allocate memory for the object + private properties 
  privateThis = (dllprivate *)secureAllocate(sizeof(*privateThis));
  if(privateThis == NULL){
    logEvent("Error", "Failed to allocate memory for linked list");
    return NULL; 
  }
  
  //initialize public methods
  privateThis->publicDll.spawnSlot       = &spawnSlot; 
  privateThis->publicDll.withdrawPointer = &withdrawPointer;
  privateThis->publicDll.depositPointer  = &depositPointer; 
  
  //initialize private properties 
  privateThis->head   = NULL; 
  privateThis->tail   = NULL;
  privateThis->count  = 0; 
  

  //return public object
  return (dllObject *)privateThis;
}



/************* PUBLIC METHODS ************/


// LIST SCAFFOLDING  

/*
 * spawnSlot adds an empty slot to the list, returns 0 on error and the current slot count on success
 * TODO currently error handling on this can put it into a screwed state of being how I intend to use it,
 * however I plan to redo error handling, so will leave it for now 
 */
static int spawnSlot(dllObject *this)
{
  dllPrivate *private      = NULL 
  dllSlot    *slot         = NULL;
  int        insertSuccess = 0;
  
  //first some sanity checking
  if(this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0; 
  }
  
  private = (dllPrivate *)this;
  
  //sanity checks passed so allocate a new slot
  slot = (dllSlot *)secureAllocate(sizeof(*slot));
  if(slot == NULL){
    logEvent("Error", "Failed to create dll slot");
    return 0;
  }
  
  slot->pointer = NULL; 

  //insert the open slot to the head of the list
  insertSuccess = (private->head == NULL) ? insertInitial(this, slot) : insertHead(this, slot); 
  
  if(!insertSuccess){
    secureFree(&slot, sizeof(*slot)); 
    logEvent("Error", "Failed to insert slot into list");
    return 0; 
  }
  
  private->count += 1; 
  
  
  return private->count; 
}



// LIST DATA

/* 
 * Checks slots from head and returns the pointer held by the first slot that holds a pointer to something besides NULL, or a NULL pointer on error or all slots empty 
 * NOTE: for thread safety access to this must be blocked with a mutex (do so at a higher level of abstraction) 
 */
static void *withdrawPointer(dllObject *this)
{
  dllPrivate *private = NULL;
  dllSlot    *slot    = NULL;
  void       *holder  = NULL; 
  
  if(this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return NULL; 
  }
  
  private = (privateDll *)this;
  
  for(slot = private->head ; slot ; slot = slot->next){
    if(slot->pointer != NULL){
      holder     = slot->pointer;
      slot->pointer = NULL;
      return holder; 
    }
  }
  
  return NULL; 
}


//checks slots from head and adds item to first open slot available, returns -1 on error, 1 on success, and 0 if no open slots
static int depositPointer(dllObject *this, void *pointer)
{
  dllPrivate *private = NULL;
  dllSlot    *slot    = NULL;
  
  if(this == NULL || pointer == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return -1; 
  }
  
  private = (dllPrivate *)this;
  
  for(slot = private->head ; slot ; slot = slot->next){
    if(slot->pointer == NULL){
      slot->pointer = pointer;
      return 1; 
    }
  }
  
  return 0; 
}




/**** PRIVATE METHODS *****/ 


// LIST SCAFFOLDING  

/*
 * insertInitial adds the first slot to a list that has no slots in it. Returns 0 on error and 1 on success. 
 */
static inline int insertInitial(dllObject *this, dllSlot *slot)
{
  privateDll *private = NULL;
  
  if(this == NULL || slot == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  private = (dllPrivate *)this;
  
  private->head     = slot;
  private->tail     = slot;
  slot->next        = NULL;
  slot->previous    = NULL;    
  
  return 1;
}


/*
 * insertHead adds a slot to the head of a list that has at least one slot already in it. Returns 0 on error and 1 on success. 
 */
static inline int insertHead(dllObject *this, dllSlot *slot)
{
  privateDll *private = NULL;
  
  if(this == NULL || slot == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  private = (dllPrivate *)this;
  
  private->head->previous = slot;
  slot->next              = private->head;
  slot->previous          = NULL;
  private->head           = slot;
  
  return 1;
}