#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

#include "dataContainer.h"
#include "router.h"
#include "dll.h"
#include "memoryManager.h"
#include "diskFile.h"
#include "server.h"
#include "ogEnums.h"

static void *processConnection(void *connectionObjectV);
int prepareSharedFiles(serverObject *this);
static int serverListen(serverObject *this);


static int sendFileNotFound(connectionObject *connection);
static int sendNextRequestedFile(connectionObject *connection);
static dataContainerObject *getRequestedFilename(connectionObject *connection);

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
serverObject *newServer(char *sharedFolderPath, char *bindAddress, int listenPort, uint16_t maxMemoryCacheMegabytes)
{
  serverObject *this; 
    
  if(sharedFolderPath == NULL || bindAddress == NULL){
    printf("Error: Something was NULL that shouldn't have beenaaaaaa\n");
    return NULL;
  }
  
  this = (serverObject *)secureAllocate(sizeof(*this));
  if(this == NULL){ 
    printf("Error: Failed to allocate memory to instantiate server\n");
    return NULL; 
  }
  
  
  //simple unsigned integer wrap protection, limits cache to slightly over four gigabytes
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
static int serverListen(serverObject *this)
{
  pthread_t        processingThread;
  connectionObject *connection;
  
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  while(1){  
    //create a connection object 
    connection = newActiveConnection(this);
    if(connection == NULL){
      printf("Error: server failed to new a connection object (probably because this was NULL)\n");
      return 0;
    }
    
    //block until a connecting client uses the connection object 
    if( !connection->router->setSocket( connection->router, this->listeningRouter->getConnection(this->listeningRouter) ) ){
      printf("Error: Failed to set active connection router socket\n");
      return 0;
    }
    
    if( pthread_create(&processingThread, NULL, processConnection, (void*)connection) != 0 ){     
      if( !connection->router->destroyRouter(&connection->router) ){
	printf("Error: Failed to destroy router object\n");
	return 0;	
      }
     
      if( !secureFree(&connection, sizeof(*connection)) ){
        printf("Error: Failed to free connectionObject object\n");
        return 0;
      }
    }
  }
  
}







/****************** PRIVATE METHODS *******************/


static void *processConnection(void *connectionV)
{
  connectionObject *connection;
  uint32_t         totalBytesize;
  uint32_t         filenameBytesize; 
  
  //give the connectionV the correct cast
  connection = (connectionObject*)connectionV;
  
  //basic sanity checking
  if( connection == NULL || connection->router == NULL || connection->server == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    goto cleanup; 
  }
  
  //get the total incoming bytesize, perform basic sanity check
  totalBytesize = connection->router->getIncomingBytesize(connection->router); 
  if(totalBytesize > MAX_REQUEST_STRING_BYTESIZE || totalBytesize == 0){
    printf("Error: Client wants to send more bytes than allowed, or error in getting total request bytesize\n");
    goto cleanup; 
  }
  
  //send all the requested files
  for(filenameBytesize = 0; totalBytesize > 0; totalBytesize -= filenameBytesize + sizeof(uint32_t)){ 
    filenameBytesize = sendNextRequestedFile(connection);
    
    //should we check for this?
    if(totalBytesize - filenameBytesize < 0){
      printf("Error: Client sent more bytes than it said it was going to\n");
      goto cleanup; 
    }
    
    if(filenameBytesize == -1){
      printf("Error: Failed to send file to client\n");
      goto cleanup;  
    }
  }
    
  cleanup:  
    if(connection != NULL && !connection->destroyConnectionObject(&connection)) printf("Error: Failed to destroy active connection object\n"); 
    return NULL;  
}
  
//returns pointer to NULL on error   
static dataContainerObject *getRequestedFilename(connectionObject *connection)
{
  uint32_t fileIdBytesize;
  
  fileIdBytesize = connection->router->getIncomingBytesize(connection->router); 
  if(fileIdBytesize > MAX_FILE_ID_BYTESIZE || fileIdBytesize == 0){
   return NULL;  
  }
        
  return connection->router->receive(connection->router, fileIdBytesize);  
}
  
//returns bytesize of sent filename (or -1 on error); 
static int sendNextRequestedFile(connectionObject *connection)
{
  dataContainerObject *currentFilename = NULL; 
  dataContainerObject *outgoingFile    = NULL; 
  uint32_t            filenameBytesize = 0;
  
  if(connection == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    goto error; 
  }
  
  currentFilename = getRequestedFilename(connection); 
  if(currentFilename == NULL){
   printf("Error: Failed to get file ID from client\n");
   goto error; 
  }
  
  /*
  //NOTE that we don't want to destroy this because it is a pointer to the file on the cache linked list (or NULL if it isn't on it)
  //NOTE TODO stop getting cachedSharedFiles from connection object (which is obsoleted) and rather use static file global heap variable 
  outgoingFile = connection->server->cachedSharedFiles->getId(connection->server->cachedSharedFiles, currentFilename->data, currentFilename->bytesize);
  */
  
  if(!outgoingFile && !sendFileNotFound(connection)){
    printf("Error: Failed to send file not found to client\n");
    goto error; 
  }
  
  if( !connection->router->transmitBytesize(connection->router, outgoingFile->bytesize) ){
    printf("Error: Failed to transmit file bytesize to client\n");
    goto error;
  }
  
  if( !connection->router->transmit(connection->router, outgoingFile->data, outgoingFile->bytesize) ){
    printf("Error: Failed to transmit file to client\n");
    goto error; 
  }
  

  filenameBytesize = currentFilename->bytesize; 
  currentFilename->destroyDataContainer(&currentFilename);
  return filenameBytesize; 
  
  error:
   if(currentFilename != NULL) currentFilename->destroyDataContainer(&currentFilename);
   return -1; 
}
  
  
static int sendFileNotFound(connectionObject *connection)
{
  if( !connection->router->transmitBytesize( connection->router, strlen("not found") ) ){ 
    return 0;
  }
      
  if( !connection->router->transmit( connection->router, "not found", strlen("not found") ) ){
    return 0; 
  }
    
  return 1; 
}


//returns 0 on error
int prepareSharedFiles(serverObject *this)
{
  uint32_t            currentlyCachedBytes;
  DIR                 *directory; 
  struct dirent       *fileEntry; 
  diskFileObject      *diskFile; 
  long                currentFileBytesize; 
  
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
    
    diskFile = newDiskFile();
    if(diskFile == NULL){
      printf("Error: Failed to create diskFile object\n");
      return 0; 
    }
    
    if( !diskFile->dfOpen(diskFile, this->sharedFolderPath, fileEntry->d_name, "r") ){
      printf("Error: Failed to open file on disk\n");
      return 0; 
    }
    
    currentFileBytesize = diskFile->dfBytesize(diskFile); 
    if(currentFileBytesize == -1){
      printf("Error: Failed to get shared file bytesize\n");
      return 0;
    }
    
    //if adding the file to the cache will exhaust the cache space then don't add it to the linked list
    if( (currentlyCachedBytes + currentFileBytesize) > this->maxMemoryCacheBytesize ){ //TODO keep track of size of cache in fileBank? 
      printf("Notice: Adding file %s will exhaust available cache, skipping\n", fileEntry->d_name); 
      diskFile->closeTearDown(&diskFile);
      continue; 
    }
    
    

    /*
    if( !this->cachedSharedFiles->insert(this->cachedSharedFiles, DLL_HEAD, fileEntry->d_name, strlen(fileEntry->d_name), diskFile->diskFileRead(diskFile, ), diskFile->getBytesize(diskFile), FILE_START) ){
      printf("Error: Failed to insert file into cached file list!");
      return 0; 
    }
    */
    
    currentlyCachedBytes += currentFileBytesize;
    if(!diskFile->closeTearDown(&diskFile)){
      printf("Error: Failed to close the disk file\n");
      return 0; 
    }
  }
  
  return 1; 
}