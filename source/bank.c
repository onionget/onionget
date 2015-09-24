#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

#include "dataContainer.h"
#include "bank.h"
#include "memoryManager.h"
#include "ogEnums.h"


typedef struct bankSlot
{
  struct bankSlot   *previous;
  struct bankSlot   *next;
  void             *pointer; 
}bankSlot;


typedef struct bankPrivate{
  bankObject        publicBank; 
  struct bankSlot   *head;
  struct bankSlot   *tail;
  uint32_t         count; //deal with overflow potential ! TODO
}bankPrivate;


//public methods
static int        spawnSlot(bankObject *this);
static void       *withdraw(bankObject *this);
static int        deposit(bankObject *this, void *pointer);

//private methods 
static inline int insertInitial(bankObject *this, bankSlot *slot);
static inline int insertHead(bankObject *this, bankSlot *slot);


/************ OBJECT CONSTRUCTOR ******************/


/*
 * newBank returns NULL on error, or a new doubly linked list object (with head and tail NULL) on success 
 */
bankObject* newBank(void)
{
  bankPrivate *privateThis = NULL;
  
  //allocate memory for the object + private properties 
  privateThis = (bankPrivate *)secureAllocate(sizeof(*privateThis));
  if(privateThis == NULL){
    logEvent("Error", "Failed to allocate memory for linked list");
    return NULL; 
  }
  
  //initialize public methods
  privateThis->publicBank.spawnSlot       = &spawnSlot; 
  privateThis->publicBank.withdraw = &withdraw;
  privateThis->publicBank.deposit  = &deposit; 
  
  //initialize private properties 
  privateThis->head   = NULL; 
  privateThis->tail   = NULL;
  privateThis->count  = 0; 
  

  //return public object
  return (bankObject *)privateThis;
}



/************* PUBLIC METHODS ************/


// LIST SCAFFOLDING  

/*
 * spawnSlot adds an empty slot to the list, returns 0 on error and the current slot count on success
 * TODO currently error handling on this can put it into a screwed state of being how I intend to use it,
 * however I plan to redo error handling, so will leave it for now 
 */
static int spawnSlot(bankObject *this)
{
  bankPrivate *private      = NULL 
  bankSlot    *slot         = NULL;
  int        insertSuccess = 0;
  
  //first some sanity checking
  if(this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0; 
  }
  
  private = (bankPrivate *)this;
  
  //sanity checks passed so allocate a new slot
  slot = (bankSlot *)secureAllocate(sizeof(*slot));
  if(slot == NULL){
    logEvent("Error", "Failed to create bank slot");
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
 * NOTE: for thread safety access to this must be blocked with a mutex (do so at a higher level of abstraction) TODO should put at tail when NULL 
 */
static void *withdraw(bankObject *this)
{
  bankPrivate *private = NULL;
  bankSlot    *slot    = NULL;
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
static int deposit(bankObject *this, void *pointer)
{
  bankPrivate *private = NULL;
  bankSlot    *slot    = NULL;
  
  if(this == NULL || pointer == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return -1; 
  }
  
  private = (bankPrivate *)this;
  
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
static inline int insertInitial(bankObject *this, bankSlot *slot)
{
  privateDll *private = NULL;
  
  if(this == NULL || slot == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  private = (bankPrivate *)this;
  
  private->head     = slot;
  private->tail     = slot;
  slot->next        = NULL;
  slot->previous    = NULL;    
  
  return 1;
}


/*
 * insertHead adds a slot to the head of a list that has at least one slot already in it. Returns 0 on error and 1 on success. 
 */
static inline int insertHead(bankObject *this, bankSlot *slot)
{
  privateDll *private = NULL;
  
  if(this == NULL || slot == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  private = (bankPrivate *)this;
  
  private->head->previous = slot;
  slot->next              = private->head;
  slot->previous          = NULL;
  private->head           = slot;
  
  return 1;
}