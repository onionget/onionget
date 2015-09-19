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
static activeConnection *getConnectionObject(server *this);

static int sendFileNotFound(activeConnection *connection);

//0         1               2               3                    4
//[exe] [server address] [server port] [shared folder path] [memory cache megabyte size (max 4294)] 

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
    printf("Error: Something was NULL that shouldn't have beenaaaaaa\n");
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
    printf("Error: Something was NULL that shouldn't have beenaaaaaa\n");
    return 0;
  }
  
  while(1){  
    //create a connection object 
    connection = getConnectionObject(this);
    if(connection == NULL){
      printf("Error: server failed to new a connection object (probably because this was NULL)\n");
      return 0;
    }
    
    //block until a connecting client uses the connection object 
    if( !connection->connectedRouter->setSocket( connection->connectedRouter, this->listeningRouter->getConnection(this->listeningRouter) ) ){
      printf("Error: Failed to set active connection router socket\n");
      return 0;
    }
    
    if( pthread_create(&processingThread, NULL, processConnection, (void*)connection) != 0 ){     
      if( !connection->connectedRouter->destroyRouter(&connection->connectedRouter) ){
	printf("Error: Failed to destroy router object\n");
	return 0;	
      }
     
      if( !secureFree(&connection, sizeof(*connection)) ){
        printf("Error: Failed to free activeConnection object\n");
        return 0;
      }
    }
  }
  
}







/****************** PRIVATE METHODS *******************/

/*
 * Keeps trying to allocate an activeConnection object, if fails due to out of memory, sleeps one second and tries again forever
 */
static activeConnection *getConnectionObject(server *this)
{
  activeConnection *connection;
  
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have beena\n");
    return NULL; 
  }
  
  do{
    connection = (activeConnection *)secureAllocate(sizeof(*connection));
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
    break; 
  }while(1);
    
  return connection; 
}


static void *processConnection(void *connectionV)
{
  activeConnection *connection;
  uint32_t         totalBytesize;
  uint32_t         sectionBytesize; 
  dataContainer    *currentFileId;
  dataContainer    *outgoingFile; 
  
  //give the connectionV the correct cast
  connection = (activeConnection*)connectionV;
  
  //basic sanity checking
  if( connection == NULL || connection->connectedRouter == NULL || connection->server == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL; 
  }
  
  //get the total incoming bytesize, perform basic sanity check
  totalBytesize = connection->connectedRouter->getIncomingBytesize(connection->connectedRouter); 
  if(totalBytesize > MAX_REQUEST_STRING_BYTESIZE || totalBytesize == 0) goto cleanup; 
  
  //go through all of the incoming bytes and process accordingly
  for(sectionBytesize = 0 ; totalBytesize > 0; totalBytesize -= sectionBytesize + sizeof(uint32_t)){
    
    //get the bytesize of the incoming requested file name and perform basic sanity checks
    sectionBytesize = connection->connectedRouter->getIncomingBytesize(connection->connectedRouter); 
    if(sectionBytesize > MAX_SECTION_BYTESIZE || sectionBytesize == 0) goto cleanup; 
        
    currentFileId = connection->connectedRouter->receive(connection->connectedRouter, sectionBytesize); 
    if(currentFileId == NULL) goto cleanup; 
    
    outgoingFile = connection->server->cachedSharedFiles->getId(connection->server->cachedSharedFiles, currentFileId->data, currentFileId->bytesize);
    if(!outgoingFile && !sendFileNotFound(connection)) goto cleanup; 
    else if(!outgoingFile){
      currentFileId->destroyDataContainer(&currentFileId); //never NULL if made it here
      continue; 
    }
    
    currentFileId->destroyDataContainer(&currentFileId);
    
    if( !connection->connectedRouter->transmitBytesize(connection->connectedRouter, outgoingFile->bytesize) )             goto cleanup;
    if( !connection->connectedRouter->transmit(connection->connectedRouter, outgoingFile->data, outgoingFile->bytesize) ) goto cleanup; 
  }
    

  cleanup:  
   if( currentFileId != NULL && !currentFileId->destroyDataContainer(&currentFileId))   printf("Error: Failed to destroy current file ID after done with it\n");
   if( !connection->connectedRouter->destroyRouter(&connection->connectedRouter) )      printf("Error: Failed to destroy router object\n"); 
   if( !secureFree(&connection, sizeof(*connection)) )                                  printf("Error: Failed to free connection object\n"); //todo make a destroy function for activeConnection maybe even split it off into its own file
   return NULL;  
}
  
  

  
  
  
   

  





static int sendFileNotFound(activeConnection *connection)
{
  if( !connection->connectedRouter->transmitBytesize( connection->connectedRouter, strlen("not found") ) ){ 
    return 0;
  }
      
  if( !connection->connectedRouter->transmit( connection->connectedRouter, "not found", strlen("not found") ) ){
    return 0; 
  }
    
  return 1; 
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
    printf("Error: Something was NULL that shouldn't have beeaaan\n");
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