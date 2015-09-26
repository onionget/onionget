#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "fileBank.h"
#include "memoryManager.h"
#include "diskFile.h"
#include "bank.h"

//globalFileBank->withdrawById(connection->requestedFilename, filenameBytesize);

//TODO consider which mutex is actually required (or both)
static pthread_mutex_t depositLock  = PTHREAD_MUTEX_INITIALIZER; 
static pthread_mutex_t withdrawLock = PTHREAD_MUTEX_INITIALIZER;

typedef struct fileBankprivate{
  fileBankObject publicFileBank; 
  bankObject    *diskFileObjects; 
}fileBankPrivate; 

static diskFile *withdrawById(char *requestedId, uint32_t idBytesize);
static int      deposit(fileBankObject *this, diskFileObject *file);


fileBankObject *newFileBank(uint32_t slots)
{
  fileBankPrivate *privateThis = NULL;
  
  if(slots == 0){
    logEvent("Error", "Cannot create file bank with no slots");
    return NULL; 
  }
  
  privateThis = (fileBankPrivate *)secureAllocate(sizeof(*privateThis)); 
  if(privateThis == NULL){
    logEvent("Error", "Failed to allocate memory for filebank object");
    return NULL; 
  }
  
  //TODO dependency inject this?
  privateThis->diskFileObjects = newBank(slots); 
  if(privateThis->diskFileObjects == NULL){
    logEvent("Error", "Failed to create bank for file objects");
    return NULL; 
  }
  
  
  //set public methods
  privateThis->publicFileBank.deposit      = &deposit; 
  privateThis->getPointerById              = &getPointerById; 
}



//returns 0 on error and 1 on success
static int deposit(fileBankObject *this, diskFileObject *file)
{
  pthread_mutex_lock(&depositLock); 

  fileBankPrivate *private = (fileBankPrivate *)this; 

  if(private == NULL || file == NULL || private->diskFileObjects == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    pthread_mutex_unlock(&depositLock); 
    return 0; 
  }
  
  if( !private->diskFileObjects->deposit(private->diskFileObjects, file, file->id, file->idBytesize) ){ //TODO need to add id and idBytesize to file 
    logEvent("Error", "Failed to deposit a diskFile object");
    pthread_mutex_unlock(&depositLock); 
    return 0; 
  }
  
  pthread_mutex_unlock(&depositLock); 
  
  return 1; 
}



static diskFileObject *getPointerById(fileBankObject *this, char *requestedId, uint32_t idBytesize)
{
  pthread_mutex_lock(&withdrawLock); 
  
  diskFileObject *filePointer = NULL; 
  fileBankPrivate *private    = (fileBankPrivate *)this; 
  
  if(private == NULL || requestedId == NULL || idBytesize == 0){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return NULL; 
  }
  
  filePointer = (diskFileObject*) private->diskFileObjects->getPointerById(private->diskFileObjects, requestedId, idBytesize);
  
  
  if(filePointer == NULL){ //TODO better error checking coming soon
    logEvent("Error", "Incapable of finding file by ID, maybe it doesn't exist or maybe error");
    return NULL;
  }
  
  pthread_mutex_unlock(&withdrawLock); 
  
  return filePointer;   
}



