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
#include "og_enums.h"

static void *processConnection(void *activeConnectionV);
int prepareSharedFiles(server *this);
static int serverListen(server *this);


//0         1               2               3                    4
//[exe] [server address] [server port] [shared folder path] [memory cache megabyte size] 

/*
 * newServer initializes a new server object, passed a NULL terminated string sharedFolderPath which is full path to the servers shared folder
 * null terminated string bindAddress is ipv4 address to bind to, int listen port is port on bindAddress to listen on
 * 
 * maxMemoryCacheMegabytes is maximum amount of RAM to use for file cache, in megabytes, this is converted to bytes and stored in a uint32_t internally
 * and therefore to prevent unsigned integer wrapping it may not be larger than UINT32_MAX / BYTES_IN_A_MEGABYTE. TODO maybe sometime in the future
 * store bytesize of cache as uint64_t, but for now this is a clean and adequate solution, it just limits the cache size to a little over four gigabytes
 * 
 * returns pointer to server object on success, pointer to NULL on error
 */
server *newServer(char *sharedFolderPath, char *bindAddress, int listenPort, uint16_t maxMemoryCacheMegabytes)
{
  server *this; 
    
  if(sharedFolderPath == NULL || bindAddress == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL;
  }
  
  this = (server *)secureAllocate(sizeof(*this));
  if(this == NULL){ 
    printf("Error: Failed to allocate memory to instantiate server\n");
    return NULL; 
  }
  
  
  //simple unsigned integer wrap protection, limits cache to slight over four gigabytes
  if(maxMemoryCacheMegabytes > UINT32_MAX / BYTES_IN_A_MEGABYTE){
    printf("Error: maximum cache megabyte size supported is %lu, as cache is internally stored in bytes in a uint32_t\n", (unsigned long) (UINT32_MAX / BYTES_IN_A_MEGABYTE) ); 
    return NULL; 
  }
  else{
    this->maxMemoryCacheBytesize = maxMemoryCacheMegabytes * BYTES_IN_A_MEGABYTE;
  }
   
  this->sharedFolderPath         = sharedFolderPath;
  this->sharedFolderPathBytesize = strlen(this->sharedFolderPath); 
  this->bindAddress              = bindAddress;
  this->listenPort               = listenPort;
  
  
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
  
  if( !prepareSharedFiles(this) ){
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
static int serverListen(server *this)
{
  pthread_t        processingThread;
  activeConnection *connection;
  
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  while(1){  
    connection = (activeConnection*)secureAllocate(sizeof(*connection));
    if(connection == NULL){
      sleep(1);
      continue; 
    }
 
    connection->connectedRouter = newRouter(); 
    if(connection->connectedRouter == NULL){
      secureFree(&connection, sizeof(*connection)); 
      sleep(1);
      continue;
    }
    
    connection->server = this;
    
    if( !connection->connectedRouter->setSocket( connection->connectedRouter, this->listeningRouter->getConnection(this->listeningRouter) ) ){
      printf("Error: Failed to set active connection router socket\n");
      return 0;
    }
  
    
    if( pthread_create(&processingThread, NULL, processConnection, (void*)connection) ){
      
      if( !connection->connectedRouter->destroyRouter(&connection->connectedRouter) ){
	printf("Error: Failed to destroy router object\n");
	return 0;
      }
     
      if( !secureFree(&connection, sizeof(*connection)) ){
	printf("Error: Failed to free activeConnection object\n");
	return 0;
      }
   
      continue;
    }
  }
}





/****************** PRIVATE METHODS *******************/


static void *processConnection(void *connectionV)
{
  activeConnection *connection;
  dataContainer    *outgoingFile; 
  dataContainer    *fileRequestString; 
  uint32_t         incomingBytesize;
  server           *server;
  router           *connectedRouter; 
  size_t           currentSectionBytesize;
  char             *currentSection;

  if(connectionV == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL; 
  }
  
  currentSection = NULL; 
  
  connection       = (activeConnection*)connectionV; 
  server           = connection->server;
  connectedRouter  = connection->connectedRouter; 
  

  incomingBytesize  = connectedRouter->getIncomingBytesize(connectedRouter);
  if( incomingBytesize > MAX_REQUEST_STRING_BYTESIZE || incomingBytesize == 0){
    secureFree(&connection, sizeof(*connection)); 
    connectedRouter->destroyRouter(&connectedRouter);   
    return NULL; 
  }

  
  fileRequestString = connectedRouter->receive(connectedRouter, incomingBytesize); 
  if(fileRequestString == NULL){
    secureFree(&connection, sizeof(*connection)); 
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
	secureFree(&connection, sizeof(*connection)); 
        connectedRouter->destroyRouter(&connectedRouter);   
	return NULL;
      }
      
      if( !connectedRouter->transmit( connectedRouter, "not found", strlen("not found") ) ){
	secureFree(&connection, sizeof(*connection)); 
        connectedRouter->destroyRouter(&connectedRouter);   
	return NULL; 
      }
      
      continue;
    }
    
    if( !connectedRouter->transmitBytesize(connectedRouter, outgoingFile->bytesize) ){
      secureFree(&connection, sizeof(*connection)); 
      connectedRouter->destroyRouter(&connectedRouter);   
      return NULL;
    }
    
    if (!connectedRouter->transmit(connectedRouter, outgoingFile->data, outgoingFile->bytesize) ){
      secureFree(&connection, sizeof(*connection)); 
      connectedRouter->destroyRouter(&connectedRouter);   
      return NULL; 
    }
  }
  
  if( !secureFree(&connection, sizeof(*connection)) ){
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
int prepareSharedFiles(server *this)
{
  uint32_t      currentlyCachedBytes;
  DIR           *directory; 
  struct dirent *fileEntry; 
  diskFile      *diskFile; 
  long          currentFileBytesize; 
  
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
    
    //if adding the file to the cache will exhaust the cache space then don't add it to the linked list
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