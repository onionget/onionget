#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

#include "datacontainer.h"
#include "router.h"
#include "dlinkedlist.h"
#include "memorymanager.h"
#include "diskfile.h"
#include "server.h"


enum{  TERMINATING_NULL_HOP        = 1         };
enum{  MAX_REQUEST_STRING_BYTESIZE = 1000000   };
enum{  BYTES_IN_A_MEGABYTE         = 1000000   }; 


static void* processConnection(void* activeConnectionV);
int cacheSharedFiles(server* this);
static int serverListen(server* this);


//0         1               2               3                    4
//[exe] [server address] [server port] [shared folder path] [memory cache megabyte size] 


server *newServer(char *sharedFolderPath, char *bindAddress, int listenPort, uint64_t maxMemoryCacheMegabytes)
{
  server *this; 
    
  if(sharedFolderPath == NULL || bindAddress == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL;
  }
  
  this = secureAllocate(sizeof(struct server));
  if(this == NULL){ 
    printf("Error: Failed to allocate memory to instantiate server\n");
    return NULL; 
  }
  
  this->sharedFolderPath         = sharedFolderPath;
  this->sharedFolderPathBytesize = strlen(this->sharedFolderPath); 
  this->bindAddress              = bindAddress;
  this->listenPort               = listenPort;
  this->maxMemoryCacheBytesize   = maxMemoryCacheMegabytes * BYTES_IN_A_MEGABYTE; 
  
  this->cachedSharedFiles = newDll();
  if(this->cachedSharedFiles == NULL){
    printf("Error: Failed to create cached file linked list, server instantiation failed\n");
    return NULL; 
  }
  
  this->listeningRouter = newRouter();
  if(this->listeningRouter == NULL){
    printf("Error: Failed to create router object, server instantiation failed\n");
    return NULL; 
  }
  
  
  //initialize public methods
  this->serverListen = &serverListen; 
  
  if( !cacheSharedFiles(this) ){
    printf("Error: Failed to cache shared files!");
    return NULL; 
  }
  
  if( !this->listeningRouter->ipv4Listen(this->listeningRouter, this->bindAddress, this->listenPort) ){
    printf("Error: Failed to set server in a listening state\n");
    return NULL; 
  }
  
  return this;
}





/************ PUBLIC METHODS *************/

//returns 0 on error, otherwise doesn't return
static int serverListen(server* this)
{
  pthread_t        processingThread;
  activeConnection *activeConnection;
  
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  while(1){  
    activeConnection = secureAllocate(sizeof(struct activeConnection)); 
    if(activeConnection == NULL){
      sleep(1);
      continue; 
    }
 
    activeConnection->connectedRouter = newRouter(); 
    if(activeConnection->connectedRouter == NULL){
      secureFree(&activeConnection, sizeof(struct activeConnection)); 
      sleep(1);
      continue;
    }
    
    activeConnection->server = this;
    
    if( !activeConnection->connectedRouter->setSocket( activeConnection->connectedRouter, this->listeningRouter->getConnection(this->listeningRouter) ) ){
      printf("Error: Failed to set active connection router socket\n");
      return 0;
    }
  
    
    if( pthread_create(&processingThread, NULL, processConnection, (void*)activeConnection) ){
      
      if( !activeConnection->connectedRouter->destroyRouter(&activeConnection->connectedRouter) ){
	printf("Error: Failed to destroy router object\n");
	return 0;
      }
     
      if( !secureFree(&activeConnection, sizeof(struct activeConnection)) ){
	printf("Error: Failed to free activeConnection object\n");
	return 0;
      }
   
      continue;
    }
  }
}





/****************** PRIVATE METHODS *******************/


static void* processConnection(void* activeConnectionV)
{
  activeConnection *activeConnection;
  dataContainer    *outgoingFile; 
  dataContainer    *fileRequestString; 
  uint32_t         incomingBytesize;
  server           *server;
  router           *connectedRouter; 
  uint32_t         currentSectionBytesize;
  char             *currentSection;

  if(activeConnectionV == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL; 
  }
  
  currentSection = NULL; 
  
  activeConnection = activeConnectionV; //TODO why doesn't casting this work
  server           = activeConnection->server;
  connectedRouter  = activeConnection->connectedRouter; 
  

  incomingBytesize  = connectedRouter->getIncomingBytesize(connectedRouter);
  if( incomingBytesize > MAX_REQUEST_STRING_BYTESIZE || incomingBytesize == 0){
    secureFree(&activeConnection, sizeof(struct activeConnection)); 
    connectedRouter->destroyRouter(&connectedRouter);   
    return NULL; 
  }

  
  fileRequestString = connectedRouter->receive(connectedRouter, incomingBytesize); 
  if(fileRequestString == NULL){
    secureFree(&activeConnection, sizeof(struct activeConnection)); 
    connectedRouter->destroyRouter(&connectedRouter);   
    return NULL; 
  }
  
  
  //NOTE: Enforce NULL termination of request string, this is vital for security 
  memset( &(fileRequestString->data[fileRequestString->bytesize - 1]), '\0', 1); 

  
  for( currentSection = strtok( (char*)(fileRequestString->data), "/") ; currentSection != NULL ; currentSection = strtok(NULL, "/") ){
 
    currentSectionBytesize = strlen(currentSection);
    
    outgoingFile = server->cachedSharedFiles->getId(server->cachedSharedFiles, currentSection, currentSectionBytesize);
    if(!outgoingFile){
      
      if( !connectedRouter->transmitBytesize( connectedRouter, strlen("not found") ) ){
	secureFree(&activeConnection, sizeof(struct activeConnection)); 
        connectedRouter->destroyRouter(&connectedRouter);   
	return NULL;
      }
      
      if( !connectedRouter->transmit( connectedRouter, "not found", strlen("not found") ) ){
	secureFree(&activeConnection, sizeof(struct activeConnection)); 
        connectedRouter->destroyRouter(&connectedRouter);   
	return NULL; 
      }
      
      continue;
    }
    
    if( !connectedRouter->transmitBytesize(connectedRouter, outgoingFile->bytesize) ){
      secureFree(&activeConnection, sizeof(struct activeConnection)); 
      connectedRouter->destroyRouter(&connectedRouter);   
      return NULL;
    }
    
    if (!connectedRouter->transmit(connectedRouter, outgoingFile->data, outgoingFile->bytesize) ){
      secureFree(&activeConnection, sizeof(struct activeConnection)); 
      connectedRouter->destroyRouter(&connectedRouter);   
      return NULL; 
    }
  }
  
  if( !secureFree(&activeConnection, sizeof(struct activeConnection)) ){
    printf("Error: Failed to free activeConnection object\n");
    return NULL; 
  }
 
  if( !connectedRouter->destroyRouter(&connectedRouter) ){
    printf("Error: Failed to destroy router object\n");
    return NULL; 
  }

  return NULL; 
}


//returns 0 on error
int cacheSharedFiles(server* this)
{
  uint64_t      currentlyCachedBytes;
  DIR           *directory; 
  struct dirent *fileEntry; 
  diskFile      *diskFile; 
  long int      currentFileBytesize; 
  
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
   
  directory = opendir( (const char*) this->sharedFolderPath );
  if(directory == NULL){
    printf("Error: Failed to open shared folder\n");
    return 0;
  }
  
  for( currentlyCachedBytes = 0 ; ( fileEntry = readdir(directory) ); ){
   
    if( !strcmp(fileEntry->d_name, ".") || !strcmp(fileEntry->d_name, "..") ){
      continue;
    }
    
    diskFile = newDiskFile(this->sharedFolderPath, fileEntry->d_name, "r");
    if(diskFile == NULL){
      printf("Error: Failed to open file\n");
      return 0; 
    }
    
    currentFileBytesize = diskFile->getBytesize(diskFile); 
    if(currentFileBytesize == -1){
      printf("Error: Failed to get shared file bytesize\n");
      return 0;
    }
    
    //if adding the file to the cache will exhaust the cache space then skip it and try the next file
    if( (currentlyCachedBytes + currentFileBytesize) > this->maxMemoryCacheBytesize ){
      printf("Notice: Adding file %s will exhaust available cache, skipping\n", fileEntry->d_name); 
      diskFile->closeTearDown(&diskFile);
      continue; 
    }

    if( !this->cachedSharedFiles->insert(this->cachedSharedFiles, DLL_HEAD, fileEntry->d_name, strlen(fileEntry->d_name), diskFile->diskFileRead(diskFile)) ){
      printf("Error: Failed to insert file into cached file list!");
      return 0; 
    }
    
    currentlyCachedBytes += currentFileBytesize;
    if(!diskFile->closeTearDown(&diskFile)){
      printf("Error: Failed to close the disk file\n");
      return 0; 
    }
  }
  
  return 1; 
}