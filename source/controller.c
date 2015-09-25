#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "client.h"
#include "server.h"
#include "ogEnums.h" 

//                            0          1           2                3                   4                      5                                6       7             8+ 
//client input format :  [./onionGet] [client] [tor bind address] [tor listen port]  [onion address]       [onion port]                    [operation] [save path] [filenames...] 
//server input format :  [./onionGet] [server] [server address]   [server port]      [shared folder path]  [memory cache megabyte size] 

/*
static int initializeClient(int argc, char *argv[]);
static int clientOperationValid(char* operation);
static int clientSanityCheck(char *onionPort, char *onionAddress, char *operation);
static int showHelpExit(void);
static int serverSanityCheck(int argc, char *bindAddress, char *listenPort);
static int initializeServer(int argc, char *argv[]);
static int systemSanityCheck(); 
*/

//TODO refactor this to take into account some functions never return, and clean up in general where showhelpexit is and how it works etc

int main(int argc, char *argv[])
{
  /*
  int successIndicated = 0; 
  
  if(!systemSanityCheck()){ 
    logEvent("Error", "System incompatibility detected"); 
    exit(1);
  }
  
  if      (argc < 2)                                                     showHelpExit(); 
  else if (!strcmp(argv[CLIENT_OR_SERVER], "client")) successIndicated = initializeClient(argc, argv);
  else if (!strcmp(argv[CLIENT_OR_SERVER], "server")) successIndicated = initializeServer(argc, argv); 
  else                                                                   showHelpExit();     
  
  if(!successIndicated){
    logEvent("Error", "Operation Failed");
    return 0; 
  }
  
  logEvent("Notice", "Operation Completed"); //note that server blocks and will never get here on success
  */
  return 0;
}


/*
//NOTE: Because we mmap files for reading and writing, and need to use offsets, and mmap offsets need to be multiples of
//the system page size, currently only supporting standard page sizes (all popular architectures normal page sizes supported
//and several possible huge page sizes supported as well), TODO add support for arbitrary page sizes, relevant files are 
//ogEnums (FILE_CHUNK_BYTESIZE = 2097152), and the files that use this (client, for determining how much incoming file data to write to
//the drive at a time, and {file cache related files} for determining how much file data to read in at a time. 
static int systemSanityCheck()
{
  long pageBytesize;  
  pageBytesize = sysconf(_SC_PAGE_SIZE);
  
  if( pageBytesize != 4096 && pageBytesize != 8192 && pageBytesize != 65536 && pageBytesize != 262144 && pageBytesize != 1048576 && pageBytesize != 2097152){
    logEvent("Error", "Currently only systems using 4096, 8129, 65536, 262144, 1048576, and 2097125 byte page sizes are currently supported");
    return 0;
  }
  
  return 1;
}
*/


/**** CLIENT FUNCTIONS ***/

/*
//returns 0 on error
static int initializeClient(int argc, char *argv[])
{
  clientObject   *client;
  uint64_t       fileCount;
  
  if( argv[C_TOR_BIND_ADDRESS] == NULL || argv[C_TOR_PORT] == NULL || argv[C_ONION_ADDRESS] == NULL || argv[C_ONION_PORT] == NULL || argv[C_OPERATION] == NULL || argv[C_DIR_PATH] == NULL || argv[C_FIRST_FILE_NAME] == NULL ){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0; 
  } 
  
  fileCount = argc - C_FIXED_CLI_INPUTS;
  
  if( !clientSanityCheck(argv[C_ONION_PORT], argv[C_ONION_ADDRESS], argv[C_OPERATION]) ){
    showHelpExit();
  }
    
  client = newClient( argv[C_TOR_BIND_ADDRESS], argv[C_TOR_PORT], argv[C_ONION_ADDRESS], argv[C_ONION_PORT], argv[C_OPERATION], argv[C_DIR_PATH], &argv[C_FIRST_FILE_NAME], fileCount); 
  
  
  if( !client->executeOperation(client) ){
    logEvent("Error", "Client operation failed");
    return 0; 
  }
 
  logEvent("Notice", "Client operation success");
  return 1;
}
*/




/*** SERVER FUNCTIONS ****/ 

/*

//TODO check int types not using uint64_t anymore 
static int initializeServer(int argc, char *argv[])
{
  serverObject *server;
  int          listenPort;
  uint64_t     maxMemoryCacheMegabytes;
  
  if( argv[S_BIND_ADDRESS] == NULL || argv[S_LISTEN_PORT] == NULL || argv[S_DIR_PATH] == NULL || argv[S_MEM_MEGA_CACHE] == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
      
  if( !serverSanityCheck(argc, argv[S_BIND_ADDRESS], argv[S_LISTEN_PORT]) ){
    showHelpExit();
  }
  
  listenPort              = (int)      strtoll( argv [ S_LISTEN_PORT    ] , NULL , 10 );
  maxMemoryCacheMegabytes = (uint64_t) strtoll( argv [ S_MEM_MEGA_CACHE ] , NULL , 10 );
  
  
  server = newServer( argv[S_DIR_PATH], argv[S_BIND_ADDRESS], listenPort, maxMemoryCacheMegabytes );
  if(server == NULL){
    logEvent("Error", "Failed to instantiate server object");
    return 0;
  }
  
  server->serverListen(server); //note blocks forever
  
  return 1; 
}


//returns 0 on error 1 on success
//TODO: Consider regex checks of all server initialization values? NOTE: assumes ipv4 style addresses
static int serverSanityCheck(int argc, char *bindAddress, char *listenPort)
{
  if(bindAddress == NULL || listenPort == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0; 
  }
  
  if(argc != S_FIXED_CLI_INPUTS){
    logEvent("Error", "Invalid number of arguments for server");
    return 0; 
  }

  //note moved other checks (bindaddress and listen port) to server.c TODO clean up
  
  return 1;
}
*/

/**** GENERAL FUNCTIONS *****/
/*
static int showHelpExit()
{
  printf("\n ------ SYNTAX ------ \n\n");
  printf("Client Syntax: [./onionGet] [\"client\"] [tor bind address] [tor listen port] [onion address] [onion port] [operation] [save path]\n\n");
  printf("Client Operations: [\"--get\"] [filenames] ----- gets the files named from the server\n\n");
  printf("Server Syntax: [./onionGet] [\"server\"] [bind address] [listen port] [shared folder path] [memory cache megabytes (max 4294)]\n\n");
  exit(0); 
}
*/