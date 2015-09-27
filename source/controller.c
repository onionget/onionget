#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <curses.h>
#include <signal.h>


#include "client.h"
#include "server.h"
#include "ogEnums.h" 
#include "macros.h"
#include "router.h"

//                            0          1           2                3                   4                      5                                6       7             8+ 
//client input format :  [./onionGet] [client] [tor bind address] [tor listen port]  [onion address]       [onion port]                    [operation] [save path] [filenames...] 
//server input format :  [./onionGet] [server] [server address]   [server port]      [shared folder path]  [memory cache megabyte size] 


static int systemSanityCheck(void); 
static int *clientGetFiles(char *torBindAddress, char *torPort, char *onionAddress, char *onionPort, char *dirPath, char **fileNames, uint32_t fileCount);
static int serverServeFiles(const char *sharedFolderPath, uint32_t maxCacheMegabytes, char *bindAddress, char *listenPort, uint32_t maxSharedFiles, uint32_t maxConnections);



//argv[C_TOR_BIND_ADDRESS] == NULL || argv[C_TOR_PORT] == NULL || argv[C_ONION_ADDRESS] == NULL || argv[C_ONION_PORT] == NULL || argv[C_OPERATION] == NULL || argv[C_DIR_PATH] == NULL || argv[C_FIRST_FILE_NAME] == NULL

//  argv[S_BIND_ADDRESS] == NULL || argv[S_LISTEN_PORT] == NULL || argv[S_DIR_PATH] == NULL || argv[S_MEM_MEGA_CACHE] == NULL
//  strtoll( argv [ S_LISTEN_PORT    ] , NULL , 10 );



int main()
{

  //TODO hunt down sanity checking components and consider putting them here
  

  return 0;
}



/**** Client Initialization Functions ****/

//TODO maybe pass a client in, and implement reinitialization and similar functions for client objects, if other functionalities are intended to be added
static int *clientGetFiles(char *torBindAddress, char *torPort, char *onionAddress, char *onionPort, char *dirPath, char **fileNames, uint32_t fileCount) //TODO make structs for these and validation functions and new to null etc etc 
{
  diskFile     *clientFileInterface; 
  routerObject *clientRouter;
  clientObject *client;
  
  clientFileInterface = newDiskFile(); 
  if(clientFileInterface == NULL){
    logEvent("Error", "Failed to allocate the client file interface");
    return NULL; 
  }
  
  clientRouter = newRouter();
  if(clientRouter == NULL){
    logEvent("Error", "Failed to allocate a router object for the client");
    return NULL; 
  }
  
  client = newClient(router);
  if(client == NULL){
    logEvent("Error", "Failled to allocate a client object");
    return NULL; 
  }
  
  if( !client->initializeSocks(client, torBindAddress, torPort) ){
    logEvent("Error", "Failed to initialize socks");
    return NULL;
  }
  
  if( !client->establishConnection(client, onionAddress, onionPort) ){
    logEvent("Error", "Failed to establish connection to onion server");
    return NULL; 
  }
  
  if( !getFiles(client, dirPath, fileNames, fileCount, clientFileInterface) ){
    logEvent("Error", "Failed to get files from onion server");
    return NULL; 
  }
  
  return 1;
}



/**** Server Initialization Functions *****/ 

//TODO maybe pass a server in, and implement reinitialization and similar functions for server objects (also make non singleton!), if other functionalities are intended to be added
static int serverServeFiles(const char *sharedFolderPath, uint32_t maxCacheMegabytes, char *bindAddress, char *listenPort, uint32_t maxSharedFiles, uint32_t maxConnections)
{
  routerObject     *serverRouter;
  serverObject     *server; 
  diskFileObject   *fileBank         [maxSharedFiles];
  connectionObject *connectionBank   [maxConnections]; 
  
  serverRouter = newRouter();
  if(serverRouter == NULL){
    logEvent("Error", "Failed to allocate a router object for the server"); 
  }
  
  initializeFileBank(diskFileBank); //never fails
  
  if( !initializeConnectionBank(connectionBank) ){
    logEvent("Error", "Failed to initialize connection bank");
    return 0;
  }
  
  server = newServer(serverRouter, fileBank, maxSharedFiles, connectionBank, maxConnections);
  if(server == NULL){
    logEvent("Error", "Failed to allocate a server object");
    return 0; 
  }
    
  if( !server->serve(sharedFolderPath, maxCacheMegabytes, bindAddress, listenPort) ){ //NOTE Doesn't return on success
    logEvent("Error", "Failed to start serving the shared filed");
    return 0;
  }
  

  return 1; //doesn't return on success
}


static void initializeFileBank(fileBankObject **fileBank)
{
  uint32_t fill = MAX_FILES_SHARED;
  while(fill--) fileBank[fill] = NULL;
}

static int initializeConnectionBank(connectionBankObject **connectionBank)
{
  uint32_t fill       = MAX_CONNECTIONS_ALLOWED;
  uint32_t errorCheck = MAX_CONNECTIONS_ALLOWED;
  while(fill--) connectionBank[fill] = newConnection(); 
  while(errorCheck--) if(connectionBank[errorCheck] == NULL) return 0;  
  return 1; 
}







//general functions

//NOTE: Because we mmap files for reading and writing, and need to use offsets, and mmap offsets need to be multiples of
//the system page size, currently only supporting standard page sizes 
static int systemSanityCheck(void)
{
  long pageBytesize;  
  pageBytesize = sysconf(_SC_PAGE_SIZE);
  
  if( pageBytesize != 4096 && pageBytesize != 8192){
    logEvent("Error", "Currently only systems using 4096 or 8129 byte page sizes are supported");
    return 0;
  }
  
  return 1;
}