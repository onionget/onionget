#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "bank.h"
#include "memoryManager.h"
#include "ogEnums.h"

/*
 * TODO consider replacing bank back end with just a void** seeing as bank is static size after creation for thread safety anyway, but move on to 
 * making this compile with all the changes and having it working first before going a different direction with it
 */

typedef struct bankSlot
{
  struct bankSlot   *previous;
  struct bankSlot   *next;
  void              *pointer;
  char              *id;
  uint32_t          idBytesize; 
}bankSlot;


typedef struct bankPrivate{
  bankObject        publicBank; 
  struct bankSlot   *head;
  struct bankSlot   *tail;
  uint32_t          count; //deal with overflow potential ! TODO
}bankPrivate;


//public methods
static void *withdraw(bankObject *this);
static int  deposit(bankObject *this, void *pointer, char *id, uint32_t idBytesize);
static void *getPointerById(bankObject *this, char *id, uint32_t idBytesize);

//private methods 
static int        spawnSlot(bankObject *this);
static inline int insertInitial(bankObject *this, bankSlot *slot);
static inline int insertHead(bankObject *this, bankSlot *slot);
static inline int insertTail(bankObject *this, bankSlot *slot);
static bankSlot   *unlinkTail(bankObject *this);
static bankSlot   *unlinkHead(bankObject *this);
static bankSlot   *unlinkArbitrary(bankObject *this, bankSlot *slot);
static int        moveToTail(bankObject *this, bankSlot *slot);

/************ OBJECT CONSTRUCTOR ******************/


/*
 * newBank returns NULL on error, or a new doubly linked list object (with head and tail NULL) on success 
 */
bankObject* newBank(uint32_t slots)
{
  bankPrivate *privateThis = NULL;
  
  //allocate memory for the object + private properties 
  privateThis = (bankPrivate *)secureAllocate(sizeof(*privateThis));
  if(privateThis == NULL){
    logEvent("Error", "Failed to allocate memory for linked list");
    return NULL; 
  }
  
  //initialize public methods
  privateThis->publicBank.withdraw       = &withdraw;
  privateThis->publicBank.deposit        = &deposit; 
  privateThis->publicBank.getPointerById = &getPointerById;
  
  
  //initialize private properties 
  privateThis->head   = NULL; 
  privateThis->tail   = NULL;
  privateThis->count  = 0;  //TODO not sure if I ever use this maybe remove it
  
  this->count = slots; 
  
  while(slots--){
    spawnSlot((bankObject*)privateThis);
  }
  
 
  //return public object
  return (bankObject *)privateThis;
}



/************* PUBLIC METHODS ************/


/* 
 * Checks slots from head and returns the pointer held by the first slot that holds a pointer to something besides NULL, or a NULL pointer on error or all slots empty 
 * NOTE: for thread safety access to this must be blocked with a mutex (do so at a higher level of abstraction). Withdraw removes from this list.  
 */
static void *withdraw(bankObject *this)
{
  bankPrivate *private = NULL;
  bankSlot    *slot    = NULL;
  void       *holder   = NULL; 
  
  if(this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return NULL; 
  }
  
  private = (bankPrivate *)this;
  
  for(slot = private->head ; slot ; slot = slot->next){
    if(slot->pointer != NULL){
      holder          = slot->pointer;
      slot->pointer   = NULL;
      
      if( !moveToTail(slot) ){
	logEvent("Error", "Failed to move slot to tail of list"); //TODO we only want to do this with connection objects, let's rethink if both should use the same back end
	return NULL; 
      }
      
      return holder; 
    }
  }
  
  return NULL; 
}


static void *getPointerById(bankObject *this, char *id, uint32_t idBytesize)
{
  bankPrivate *private = NULL;
  bankSlot    *slot    = NULL;
  
  if(this == NULL || id == NULL || idBytesize == 0){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return NULL;
  }
  
  private = (bankPrivate *)this;
  
  for(slot = private->head ; slot ; slot = slot->next){   
    if     ( slot->pointer == NULL            ) continue;  
    else if( !memcmp(slot->id, id, idBytesize)) return slot->pointer;  
  }
    
  return NULL;     
}


//checks slots from head and adds item to first open slot available, returns -1 on error, 1 on success, and 0 if no open slots
//NOTE: for thread safety access to this must be blocked with a mutex (do so at a higher level of abstraction) 
static int deposit(bankObject *this, void *pointer, char *id, uint32_t idBytesize)
{
  bankPrivate *private = NULL;
  bankSlot    *slot    = NULL;
  
  if(this == NULL || pointer == NULL || id == NULL || idBytesize = 0){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return -1; 
  }
  
  private = (bankPrivate *)this;
  
  for(slot = private->head ; slot ; slot = slot->next){
    if(slot->pointer == NULL){
      slot->pointer    = pointer;
      slot->id         = id;
      slot->idBytesize = idBytesize; 
      return 1; 
    }
  }
  
  return 0; 
}







/**** PRIVATE METHODS *****/ 

//NOTE Many of these functions must remain private to maintain thread safety


/*
 * spawnSlot adds an empty slot to the list, returns 0 on error and 1 on success
 */
static int spawnSlot(bankObject *this)
{
  bankPrivate *private      = NULL 
  bankSlot    *slot         = NULL;
  int         insertSuccess = 0;
  
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
  insertSuccess = insertHead(this, slot); 
  
  if(!insertSuccess){
    secureFree(&slot, sizeof(*slot)); 
    logEvent("Error", "Failed to insert slot into list");
    return 0; 
  }
  
  return 1;
}

/*
 * insertInitial adds the first slot to a list that has no slots in it. Returns 0 on error and 1 on success. 
 */
static inline int insertInitial(bankObject *this, bankSlot *slot)
{
  bankPrivate *private = NULL;
  
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
  bankPrivate *private = (bankPrivate *)this;
   
  if(this == NULL || private == NULL || slot == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  if(private->head == NULL && private->tail == NULL){
    return insertInitial(this, slot);
  }
  
  private->head->previous = slot;
  slot->next              = private->head;
  slot->previous          = NULL;
  private->head           = slot;
  
  return 1;
}


static inline int insertTail(bankObject *this, bankSlot *slot)
{
  bankPrivate *private = (bankPrivate *)this; 

  if(this == NULL || private == NULL || slot == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  //TODO write some basic tests to make sure this is having expected behavior, generally write some tests for this object + read it over again to make sure of correctness
  //primarily think about behavior when head and/or tail is NULL and not the other (or how possible this is lol). 
  if(private->head == NULL && private->tail == NULL){
    return insertInitial(this, slot);
  }
  
  
  private->tail->next = slot;
  slot->previous      = private->tail;
  slot->next          = NULL;
  private->tail       = slot; 
  
  return 1; 
}


static bankSlot *unlinkArbitrary(bankObject *this, bankSlot *slot)
{
  bankPrivate *private = (bankPrivate *)this;
  
  if(this == NULL || private == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been\n");
    return NULL;
  }
  
  if(slot == private->head){
    return unlinkHead(this);
  }
  else if(slot == private->tail){
    return unlinkTail(this); 
  }
  else{
    slot->next->previous = slot->previous;
    slot->previous->next = slot->next;
    return slot; 
  }
}


static bankSlot *unlinkHead(bankObject *this)
{
  bankPrivate *private = (bankPrivate *)this;
  bankSlot    *holder  = NULL; 
  
  if(this == NULL || private == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been\n");
    return NULL;
  }
  
  holder        = private->head; 
  private->head = private->head->next;
  
  if(private->head != NULL){
    private->head->previous = NULL; 
  }
   
  return holder; 
}


static bankSlot *unlinkTail(bankObject *this)
{
  bankPrivate *private = (bankPrivate *)this;
  bankSlot    *holder  = NULL;
  
  if(this == NULL || private == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return NULL; 
  }
  
  holder = private->tail;
  private->tail = private->tail->previous;
  
  if(private->tail != NULL){
    private->tail->next = NULL; 
  }
  
  return holder; 
}


static int moveToTail(bankObject *this, bankSlot *slot)
{
  bankPrivate *private = (bankPrivate *)this;
  
  if(this == NULL || slot == NULL || private == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  if(private->tail == slot){
    return 1; 
  }
       
  slot = unlinkArbitrary(this, slot);
  if(slot == NULL){
    logEvent("Error", "Failed to unlink slot");
    return 0; 
  }
  
  if( !insertTail(this, slot) ){
    logEvent("Error", "Failed to relocate item to list tail");
    return 0; 
  }
  
  return 1; 
}