#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "client.h"
#include "server.h"
#include "controller.h" //has enums

//                            0          1           2                3                   4                      5                                6       7             8 
//client input format :  [./onionGet] [client] [tor bind address] [tor listen port]  [onion address]       [onion port]                    [operation] [save path] [filenames...] 
//server input format :  [./onionGet] [server] [server address]   [server port]      [shared folder path]  [memory cache megabyte size] 

static int initializeClient(int argc, char *argv[]);
static int clientOperationValid(char* operation);
static int clientSanityCheck(char* onionPort, char* onionAddress, char* operation);
static int showHelpExit();
static int serverSanityCheck(int argc, char* bindAddress, char* listenPort);
static int initializeServer(int argc, char *argv[]);



//TODO refactor this to take into account some functions never return, and clean up in general where showhelpexit is and how it works etc
int main(int argc, char *argv[])
{
  int successIndicated = 0; 
  
  if      (argc < 2)                                                     showHelpExit(); 
  else if (!strcmp(argv[CLIENT_OR_SERVER], "client")) successIndicated = initializeClient(argc, argv);
  else if (!strcmp(argv[CLIENT_OR_SERVER], "server")) successIndicated = initializeServer(argc, argv); 
  else                                                                   showHelpExit();     
  
  if(!successIndicated){
    printf("Operation Failed\n");
    return 0; 
  }
  
  printf("Operation Completed\n"); //note that server blocks and will never get here on success
  return 0;
}


/**** CLIENT FUNCTIONS ***/

//returns 0 on error
static int initializeClient(int argc, char *argv[])
{
  client   *client;
  uint64_t fileCount;
  
  if( argv[C_TOR_BIND_ADDRESS] == NULL || argv[C_TOR_PORT] == NULL || argv[C_ONION_ADDRESS] == NULL || argv[C_ONION_PORT] == NULL || argv[C_OPERATION] == NULL || argv[C_DIR_PATH] == NULL || argv[C_FIRST_FILE_NAME] == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  } 
  
  fileCount = argc - C_FIXED_CLI_INPUTS;
  
  if( !clientSanityCheck(argv[C_ONION_PORT], argv[C_ONION_ADDRESS], argv[C_OPERATION]) ){
    showHelpExit();
  }
    
  client = newClient( argv[C_TOR_BIND_ADDRESS], argv[C_TOR_PORT], argv[C_ONION_ADDRESS], argv[C_ONION_PORT], argv[C_OPERATION], argv[C_DIR_PATH], &argv[C_FIRST_FILE_NAME], fileCount); 
  
  
  if( !client->executeOperation(client) ){
    printf("Error: Client operation failed\n");
    return 0; 
  }
 
  printf("Client operation success\n");
  return 1;
}


//returns 0 if invalid or NULL (which is invalid in addition to being an error), or 1 if valid
static int clientOperationValid(char* operation)
{
  char *validOperations[C_VALID_OPERATION_COUNT] = {"--get",}; 
  int testedOperations;
 
  if(operation == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  for(testedOperations = 0; testedOperations != C_VALID_OPERATION_COUNT ; testedOperations++){
    if(!strcmp(validOperations[testedOperations], operation)){
      return 1;
    }
  }
  
  return 0; 
}


//returns 0 on error
static int clientSanityCheck(char* onionPort, char* onionAddress, char* operation)
{  
  if( onionPort == NULL || onionAddress == NULL || operation == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }

  if( !clientOperationValid(operation) ){
    printf("Error: Operation %s is invalid for client", operation);
    return 0;
  }
  
  if( strlen(onionPort) > 5 || strtol(onionPort, NULL, 10) > 65535){
   printf("Error: Port of destination must be at or below 65535\n");
   return 0; 
  }
  
  
  if( strlen(onionAddress) != ONION_ADDRESS_BYTESIZE ){
   printf("Error: put onion address in form abcdefghijklmnop.onion\n");
   return 0; 
  }
  
  return 1; 
}


/*** SERVER FUNCTIONS ****/ 

static int initializeServer(int argc, char *argv[])
{
  server    *server;
  int       listenPort;
  uint64_t  maxMemoryCacheMegabytes;
  
  if( argv[S_BIND_ADDRESS] == NULL || argv[S_LISTEN_PORT] == NULL || argv[S_DIR_PATH] == NULL || argv[S_MEM_MEGA_CACHE] == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
      
  if( !serverSanityCheck(argc, argv[S_BIND_ADDRESS], argv[S_LISTEN_PORT]) ){
    showHelpExit();
  }
  
  listenPort              = (int)      strtoll( argv [ S_LISTEN_PORT    ] , NULL , 10 );
  maxMemoryCacheMegabytes = (uint64_t) strtoll( argv [ S_MEM_MEGA_CACHE ] , NULL , 10 );
  
  
  server = newServer( argv[S_DIR_PATH], argv[S_BIND_ADDRESS], listenPort, maxMemoryCacheMegabytes );
  if(server == NULL){
    printf("Failed to instantiate server object\n");
    return 0;
  }
  
  server->serverListen(server); //note blocks forever
  
  return 1; 
}


//returns 0 on error 1 on success
//TODO: Consider regex checks of all server initialization values? NOTE: assumes ipv4 style addresses
static int serverSanityCheck(int argc, char* bindAddress, char* listenPort)
{
  if(bindAddress == NULL || listenPort == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }
  
  if(argc != S_FIXED_CLI_INPUTS){
    printf("Error: Invalid number of arguments for server\n");
    return 0; 
  }
  
  if( strlen(listenPort) > 5 || strtol(listenPort, NULL, 10) > HIGHEST_VALID_PORT){
   printf("Error: Listen port must be at or below 65535\n");
   return 0; 
  }
  
  if( strlen(bindAddress) > strlen("111.111.111.111")){
    printf("Error: Bind address invalid length");
    return 0;
  }
  
  return 1;
}


/**** GENERAL FUNCTIONS *****/
static int showHelpExit()
{
  printf("\n ------ SYNTAX ------ \n\n");
  printf("Client Syntax: [./onionGet] [\"client\"] [tor bind address] [tor listen port] [onion address] [onion port] [operation] [save path]\n\n");
  printf("Client Operations: [\"--get\"] [filenames] ----- gets the files named from the server\n\n");
  printf("Server Syntax: [./onionGet] [\"server\"] [bind address] [listen port] [shared folder path] [memory cache megabytes]\n\n");
  exit(1); 
}