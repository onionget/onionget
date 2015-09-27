#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>


#include "router.h"
#include "memoryManager.h"
#include "connection.h"
#include "diskFile.h"
#include "server.h"
#include "ogEnums.h"
#include "macros.h"



//GLOBAL VARIABLES
//currently hardcoding pointer array sizes, think of a cleaner way to do this with variable number
static diskFileObject      *globalFileBank          [MAX_FILES_SHARED]; 
static connectionObject    *globalConnectionBank    [MAX_CONNECTIONS_ALLOWED];   
static routerObject        *globalServerRouter      = NULL;
static uint32_t            globalMaxCacheBytes      = 0;
//


//THREAD SAFETY LOCKS
static pthread_mutex_t connectionDepositLock  = PTHREAD_MUTEX_INITIALIZER; 
static pthread_mutex_t connectionWithdrawLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t fileDepositLock        = PTHREAD_MUTEX_INITIALIZER; 
static pthread_mutex_t fileWithdrawLock       = PTHREAD_MUTEX_INITIALIZER;
//


//PUBLIC METHODS
static int beginOperation(const char *sharedFolderPath, uint32_t maxCacheMegabytes, char *bindAddress, char *listenPort);
//


//PRIVATE METHODS
static int listenForConnections(void);
static int initializeSharedFiles(const char *sharedFolderPath, uint32_t maxCacheMegabytes);
static int initializeNetworking(char *bindAddress, char *listenPort);
static void *processConnection(void *connectionV);
static uint32_t sendNextRequestedFile(connectionObject *connection);
static int sendFileNotFound(connectionObject *connection);
//



//PRIVATE BANK METHODS
static void initializeFileBank(void);
static int depositFile(diskFileObject *file);
static diskFileObject *getFileById(char *id, uint32_t idBytesize);
static int initializeConnectionBank(void);
static connectionObject *withdrawConnection(void);
static int depositConnection(connectionObject *connection);
//



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


serverObject *newServer(void) 
{
  serverObject *this = (serverObject *)secureAllocate(sizeof(*this));
  if(this == NULL){ 
    logEvent("Error", "Failed to allocate memory to instantiate server");
    return NULL; 
  }
  
  //initialize public methods
  this->beginOperation = &beginOperation; 

  
   
  return this;
}



static int beginOperation(const char *sharedFolderPath, uint32_t maxCacheMegabytes, char *bindAddress, char *listenPort)
{
  if(sharedFolderPath == NULL || bindAddress == NULL || listenPort == NULL){ //TODO NOTE sanity check maxCachebytesize here?
    logEvent("Error", "Failed to initialize server");
    return 0; 
  }
  
  if( !initializeNetworking(bindAddress, listenPort) ){
    logEvent("Error", "Failed to initialize server");
    return 0;
  }
  
  if( !initializeSharedFiles(sharedFolderPath, maxCacheMegabytes) ){
    logEvent("Error", "Failed to initialize server");
    return 0; 
  }
  
  if( !listenForConnections() ){ 
    logEvent("Error", "Failed to start server");
    return 0;
  }
  
  //Note that this function will never return on success
  return 1; 
}



static int initializeNetworking(char *bindAddress, char *listenPort)
{
  int intPort = 0;
  
  if(bindAddress == NULL || listenPort == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  intPort = (int) strtol(listenPort, NULL, 10); //TODO check conversion safety WARNING
  
  if( strlen(listenPort) > 5 || intPort > HIGHEST_VALID_PORT){
   logEvent("Error", "Listen port must be at or below 65535");
   return 0; 
  }
  
  if( strlen(bindAddress) > strlen("111.111.111.111")){
    logEvent("Error", "Bind address invalid length");
    return 0;
  }
  
  //BRO DO YOU EVEN DEPENDENCY INJECT? TODO 
  globalServerRouter   = newRouter();
  
  if( !initializeConnectionBank() ){
    logEvent("Error", "Failed to initialize the connection bank"); 
    return 0;
  }
  
  if( !globalServerRouter->ipv4Listen( globalServerRouter, bindAddress, intPort )){
    logEvent("Error", "Failed to set server in a listening state");
    return 0; 
  }
  
  return 1; 
}



static int initializeSharedFiles(const char *sharedFolderPath, uint32_t maxCacheMegabytes)
{
  uint32_t            availableCacheBytes; 
  uint32_t            fileBytesCached; 
  DIR                 *directory; 
  struct dirent       *fileEntry; 
  diskFileObject      *diskFile; 

  if(sharedFolderPath == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  initializeFileBank();
  
  //simple unsigned integer wrap protection, limits cache to slightly over four gigabytes
  if(maxCacheMegabytes > UINT32_MAX / BYTES_IN_A_MEGABYTE){
    logEvent("Error", "maximum cache megabyte size supported is (UINT32_MAX / BYTES_IN_A_MEGABYTE), as cache is internally stored in bytes in a uint32_t");   
    return 0; 
  }
  else{
    globalMaxCacheBytes = maxCacheMegabytes * BYTES_IN_A_MEGABYTE;
  }
  
  availableCacheBytes = globalMaxCacheBytes; 

  
  directory = opendir( sharedFolderPath );
  if(directory == NULL){
    logEvent("Error", "Failed to open shared folder");
    return 0;
  }
  
  while((fileEntry = readdir(directory))){
    
    if( !strcmp(fileEntry->d_name, ".") || !strcmp(fileEntry->d_name, "..") ){
      continue;
    }
    
    diskFile = newDiskFile();
    if(diskFile == NULL){
      logEvent("Error", "Failed to create diskFile object");
      return 0; 
    }
    
   if( !diskFile->dfOpen(diskFile, sharedFolderPath, fileEntry->d_name, "r") ){ //TODO make consts in accordance with cert (everywhere)
      logEvent("Error", "Failed to open file on disk");
      return 0; 
    }
    
    fileBytesCached = diskFile->cacheBytes(diskFile, FILE_CHUNK_BYTESIZE); //TODO we need to switch from a byte paradigm to a file chunk paradigm to ensure compatibility with mmap page sizes, don't keep track of max cached bytes but rather max cached file chunks, this cache is too small
    availableCacheBytes -= fileBytesCached; //BROKEN WARNING SEE ABOVE TODO                                                                       //and should be available cache bytes TODO TODO TODO TODO TODO TODO BROKEN WARNING
    
    if( depositFile(diskFile) != 1){
      logEvent("Error", "Failed to deposit a file into shared file bank, does the shared folder have too many files in it?");
      return 0;
    }
  }

  return 1; 
}


//returns 0 on error, otherwise doesn't return
static int listenForConnections(void)
{
  pthread_t        processingThread;
  connectionObject *availableConnection; 
  
  if(globalServerRouter == NULL){
    logEvent("Error", "Global server router must be initialized prior to connection processing");
    return 0;
  }
  
  while(1){     
    //keep trying to get an available connection
    while( !(availableConnection = withdrawConnection()) ){ //NOTE wtf I don't think I actually need a mutex on withdraw only one thread accesses it
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
       
    depositConnection(connection); 
    return NULL;  
}
  
  
  
  
//returns bytesize of sent filename (or -1 on error); 
static uint32_t sendNextRequestedFile(connectionObject *connection)
{
  uint32_t       filenameBytesize = 0;
  diskFileObject *outgoingFile    = NULL; 
  uint32_t       bytesAlreadyRead = 0; 
  uint32_t       bytesToRead      = 0; 
  uint32_t       fileBytesize     = 0; //TODO eventually make uint64_t + support this in networking + client + server +disklfile etc, switch to 64 bit eventually (used many spots make sure to change all when I do it)...
  
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
     
  //random NOTE (Stop relying on strlen for anything anywhere)
  outgoingFile = getFileById(connection->requestedFilename, filenameBytesize); 
  if(!outgoingFile && !sendFileNotFound(connection)){
    logEvent("Error", "Failed to send file not found to client");
    goto error; 
  }
  
  fileBytesize = outgoingFile->getBytesize(outgoingFile);
  if(fileBytesize == -1){
    logEvent("Error", "Failed to get file bytesize");
    goto error; 
  }
  
  if( !connection->router->transmitBytesize(connection->router, fileBytesize) ){ 
    logEvent("Error", "Failed to transmit file bytesize to client");
    goto error;
  }
  
  
  for(bytesAlreadyRead = 0, bytesToRead = 0; bytesAlreadyRead < fileBytesize; bytesAlreadyRead += bytesToRead){
    bytesToRead = ( (fileBytesize - bytesAlreadyRead) < FILE_CHUNK_BYTESIZE ) ? (fileBytesize - bytesAlreadyRead) : FILE_CHUNK_BYTESIZE; 
  
    if( !outgoingFile->dfRead(outgoingFile, connection->dataCache, bytesToRead, bytesAlreadyRead) ){
      logEvent("Error", "Failed to read file bytes");
      goto error; 
    }
  
    if( !connection->router->transmit(connection->router, connection->dataCache, bytesToRead) ){ //TODO should we make a packet format that is padded and fixed size? I think so. 
      logEvent("Error", "Failed to transmit file to client");
      goto error; 
    }
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







//file bank functions
static void initializeFileBank(void)
{
  uint32_t fill = MAX_FILES_SHARED;
  while(fill--) globalConnectionBank[fill] = NULL;
}

static int depositFile(diskFileObject *file)
{
  if(file == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return -1; 
  }
  pthread_mutex_lock(&fileDepositLock);
  uint32_t slots = MAX_FILES_SHARED;
  while(slots--){
   if(globalFileBank[slots] == NULL){
     globalFileBank[slots] = file;
     pthread_mutex_unlock(&fileDepositLock); 
     return 1;
   }
  }
  pthread_mutex_unlock(&fileDepositLock); 
  return 0; 
}

static diskFileObject *getFileById(char *id, uint32_t idBytesize)
{
  pthread_mutex_lock(&fileWithdrawLock);
  
  uint32_t slots = MAX_FILES_SHARED;
  
  if(id == NULL || idBytesize == 0){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return NULL; 
  }
  
  while(slots--){
    if(memcmp(globalFileBank[slots]->getFilename(globalFileBank[slots]), id, idBytesize)){
      pthread_mutex_unlock(&fileWithdrawLock);
      return globalFileBank[slots];   
    }
  }
  
  pthread_mutex_unlock(&fileWithdrawLock);
  return NULL; 
}



//connection bank functions
static int initializeConnectionBank(void)
{
  uint32_t fill       = MAX_CONNECTIONS_ALLOWED;
  uint32_t errorCheck = MAX_CONNECTIONS_ALLOWED;
  while(fill--) globalConnectionBank[fill] = newConnection(); 
  while(errorCheck--) if(globalConnectionBank[errorCheck] == NULL) return 0;  
  return 1; 
}


static connectionObject *withdrawConnection(void)
{
  pthread_mutex_lock(&connectionWithdrawLock); 
  uint32_t         check   = MAX_CONNECTIONS_ALLOWED;
  connectionObject *holder = NULL; 
  while(check--){
    if(globalConnectionBank[check] != NULL){
      holder = globalConnectionBank[check];
      globalConnectionBank[check] = NULL;
      pthread_mutex_unlock(&connectionWithdrawLock); 
      return holder; 
    }
  }
  pthread_mutex_unlock(&connectionWithdrawLock); 
  return NULL; 
}


static int depositConnection(connectionObject *connection)
{
  pthread_mutex_lock(&connectionDepositLock); 
  uint32_t slots = MAX_CONNECTIONS_ALLOWED; 
  while(slots--){
    if(globalConnectionBank[slots] == NULL){ 
      globalConnectionBank[slots] = connection;
      pthread_mutex_unlock(&connectionDepositLock); 
      return 1;
    }
  }
  pthread_mutex_unlock(&connectionDepositLock);
  return 0;
}