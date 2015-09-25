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
#include "memoryManager.h"
#include "connectionBank.h"
#include "connection.h"
#include "diskFile.h"
#include "server.h"
#include "ogEnums.h"
#include "macros.h"

/*
 * Server follows singleton paradigm for now
 */


static FileBankObject     *globalFileBank        = NULL;
static routerBankObject   *globalConnectionBank  = NULL;
static routerObject       *globalServerRouter    = NULL;




static void *processConnection(void *connectionV);
int prepareSharedFiles(serverObject *this);
static int initializeConnectionProcessing(serverObject *this);


static int sendFileNotFound(connectionObject *connection);
static uint32_t sendNextRequestedFile(connectionObject *connection);



//singleton 

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

uint32_t maxMemoryCacheMegabytes, uint32_t maxSharedFiles, char *sharedFolderPath;


serverObject *newServer(void) //TODO NOTE: Intrinsic differences between routerBank and fileBank make them perhaps not suited for the same back end, however for now they will use it...
{
  
  serverObject *this = (serverObject *)secureAllocate(sizeof(*this));
  if(this == NULL){ 
    logEvent("Error", "Failed to allocate memory to instantiate server");
    return NULL; 
  }
  
  

  //initialize public methods
  this->initialize = &initialize; 
  
  this->initializeConnectionProccessing    = &initializeConnectionProcessing; 
  this->initializeFileCache                = &prepareSharedFiles;
  this->initializeNetworking               = &initializeNetworking; 
  
   
  return this;
}




//TODO consider dependency injecting router and connection bank etc (TODO just dependency inject all of this from controller and do sanity check there, but for now put it here) 
static int initializeNetworking(char *bindAddress, int listenPort, uint32_t connectionSlots)
{
  if(bindAddress == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  if( strlen(listenPort) > 5 || strtol(listenPort, NULL, 10) > HIGHEST_VALID_PORT){
   logEvent("Error", "Listen port must be at or below 65535");
   return 0; 
  }
  
  if( strlen(bindAddress) > strlen("111.111.111.111")){
    logEvent("Error", "Bind address invalid length");
    return 0;
  }
  
  //BRO DO YOU EVEN DEPENDENCY INJECT?
  globalServerRouter   = newRouter();
  globalConnectionBank = newConnectionBank();
  while(connectionSlots--) globalConnectionBank->deposit(newConnection()); 
  
  if( !globalServerRouter->ipv4Listen( globalServerRouter, bindAddress, listenPort )){
    logEvent("Error", "Failed to set server in a listening state");
    return 0; 
  }
  
  return 1; 
}





/************ PUBLIC METHODS *************/

//returns 0 on error, otherwise doesn't return
static int initializeConnectionProcessing(void)
{
  pthread_t        processingThread;
  connectionObject *availableConnection; 
  
  if(globalServerRouter == NULL){
    logEvent("Error", "Global server router must be initialized prior to connection processing");
    return 0;
  }
  
  while(1){     
    //keep trying to get an available connection
    while( !(availableConnection = globalConnectionBank->withdraw(globalConnectionBank)) ){ //NOTE wtf I don't think I actually need a mutex on withdraw only one thread accesses it
      //block until a connecting client needs the available router (TODO make sure globalServerRouter doesn't need error checking...)
      if( !availableConnection->router->setSocket(availableConnection->router, globalServerRouter->getConnection(globalServerRouter)) ){
	logEvent("Error", "Failed to set connection socket");
	return 0; //TODO don't want to return here add better error handling 
      }
      
      if( pthread_create(&processingThread, NULL, processConnection, (void*)availableConnection) != 0 ){
	logEvent("Error", "Failed to create thread to handle connection");
	return 0; //TODO don't want to return here add better error handling
      }
      
    }
  }
  
}







/****************** PRIVATE METHODS *******************/

static void *processConnection(void *connectionV)
{
  connectionObject *connection           = NULL; 
  uint32_t         requestBytesize       = 0;
  uint32_t         requestBytesProcessed = 0; 
  
  //cast correctly the connection
  connection = (connectionObject *)connectionV;  
  
  //basic sanity checking
  if( connection == NULL ){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return NULL; 
  }
  
  //get the total incoming bytesize, perform basic sanity check
  requestBytesize = connection->router->getIncomingBytesize(connection->router); 
  if(requestBytesize > MAX_REQUEST_STRING_BYTESIZE || requestBytesize == 0){
    logEvent("Error", "Client wants to send more bytes than allowed, or error in getting total request bytesize"); //TODO better error checking soon to come! stay tuned! 
    goto cleanup; 
  }
  
  //send all the requested files
  for(requestBytesProcessed = 0; requestBytesize > 0; requestBytesize -= requestBytesProcessed + sizeof(uint32_t)){ //+ sizeof(uint32_t) because requestBytesize includes the uint32_t seperators between file names requested
    requestBytesProcessed = sendNextRequestedFile(connection);
    
    //should we check for this?
    if(requestBytesize - requestBytesProcessed < 0){
      logEvent("Error", "Client sent more bytes than it said it was going to");
      goto cleanup; 
    }
    
    if(requestBytesProcessed == -1){
      logEvent("Error", "Failed to send file to client");
      goto cleanup;  
    }
  }
    
  cleanup:  
    if( !connection->reinitialize(connection) ){
      logEvent("Error", "Failed to reinitialize connection");
      return NULL; 
    }
       
    globalConnectionBank->deposit(connection); 
    return NULL;  
}
  
  
//returns bytesize of sent filename (or -1 on error); 
static uint32_t sendNextRequestedFile(connectionObject *connection)
{
  dataContainerObject *outgoingFile    = NULL; 
  uint32_t            filenameBytesize = 0;
  
  if(connection == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    goto error; 
  }
  
  //get the requested file name bytesize
  filenameBytesize = connection->router->getIncomingBytesize(connection->router); 
  
  //WARNING the assumption that MAX_FILE_ID_BYTESIZE is exact size of buffer connection->requestedFilename must hold true for security to be present
  if(filenameBytesize > MAX_FILE_ID_BYTESIZE || filenameBytesize == 0){ 
   return 0;  
  }
  
  //initialize filename
  if( !connection->router->receive(connection->router, connection->requestedFilename, filenameBytesize) ){
    logEvent("Error", "Failed to determine requested file name");
    return 0;
  }
     
  /*
  //NOTE that we don't want to destroy this because it is a pointer to the file on the cache linked list (or NULL if it isn't on it) (TODO think about this, depends on implementation) 
  TODO TODO TODO 
  */
  outgoingFile = globalFileBank->withdrawById(connection->requestedFilename, filenameBytesize); //TODO this needs to be implemented NOTE that we must never rely on strlen for anything always know sizes refactor out all strlens
  
  
  if(!outgoingFile && !sendFileNotFound(connection)){
    logEvent("Error", "Failed to send file not found to client");
    goto error; 
  }
  
  if( !connection->router->transmitBytesize(connection->router, outgoingFile->bytesize) ){
    logEvent("Error", "Failed to transmit file bytesize to client");
    goto error;
  }
  
  if( !connection->router->transmit(connection->router, outgoingFile->data, outgoingFile->bytesize) ){
    logEvent("Error", "Failed to transmit file to client");
    goto error; 
  }
  
  return filenameBytesize; 
  
  error:
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
  
  
  /*
   * 
   *   //simple unsigned integer wrap protection, limits cache to slightly over four gigabytes
  if(maxMemoryCacheMegabytes > UINT32_MAX / BYTES_IN_A_MEGABYTE){
    logEvent("Error", "maximum cache megabyte size supported is (UINT32_MAX / BYTES_IN_A_MEGABYTE), as cache is internally stored in bytes in a uint32_t");   
    return NULL; 
  }
  else{
    privateThis->maxMemoryCacheBytesize = maxMemoryCacheMegabytes * BYTES_IN_A_MEGABYTE;
  }
   */
  
  if(this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
   
  directory = opendir( (const char*) this->sharedFolderPath );
  if(directory == NULL){
    logEvent("Error", "Failed to open shared folder");
    return 0;
  }
  
  for( currentlyCachedBytes = 0 ; ( fileEntry = readdir(directory) ); ){
   
    if( !strcmp(fileEntry->d_name, ".") || !strcmp(fileEntry->d_name, "..") ){
      continue;
    }
    
    diskFile = newDiskFile();
    if(diskFile == NULL){
      logEvent("Error", "Failed to create diskFile object");
      return 0; 
    }
    
    if( !diskFile->dfOpen(diskFile, this->sharedFolderPath, fileEntry->d_name, "r") ){
      logEvent("Error", "Failed to open file on disk");
      return 0; 
    }
    
    currentFileBytesize = diskFile->dfBytesize(diskFile); 
    if(currentFileBytesize == -1){
      logEvent("Error", "Failed to get shared file bytesize");
      return 0;
    }
    
    //if adding the file to the cache will exhaust the cache space then don't add it to the linked list
    if( (currentlyCachedBytes + currentFileBytesize) > this->maxMemoryCacheBytesize ){ //TODO keep track of size of cache in fileBank? TODO make log function work with interpolated strings
      logEvent("Notice", "Adding file will exhaust available cache, skipping");
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
      logEvent("Error", "Failed to close the disk file");
      return 0; 
    }
  }
  
  return 1; 
}