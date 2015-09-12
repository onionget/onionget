#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


#include "router.h"
#include "datacontainer.h"
#include "memorymanager.h"
#include "diskfile.h" 
#include "client.h"

enum{  DELIMITER_BYTESIZE     = 1   };
enum{  FIRST_FILE_POSITION    = 7   };
enum{  FIXED_CLI_INPUTS       = 8   }; // [./onionGet] [tor bind address] [tor listen port] [onion address] [onion port] [operation] [save path] [filenames...] (8 including one file)
enum{  ONION_ADDRESS_BYTESIZE = 22  };

static int showHelp();
static int sanityCheck(int argc, char* secondArgument, char* onionPort, char* onionAddress);
static int establishConnection(client* this);
static int prepareFileRequestString(client* this, int argc, char* argv[]);
static uint32_t calculateFileRequestStringBytesize(int argc, char* argv[]);
static int getFiles(client* this, int argc, char *argv[]);
static int executeOperation(client* this, int argc, char* argv[]);


/*
int main(int argc, char *argv[])
{
  client *client;
  client = newClient(argc, argv);
  if(client == NULL) exit(1); 
  
  if( client->executeOperation(client, argc, argv) ){
    printf("Operation Complete");
  }
  else{
    printf("Operation Failed"); 
  }
    
  return 0; 
}
*/



/******** CONSTRUCTOR ***********/
client* newClient(int argc, char* argv[])
{
  client *this; 
  
  //make sure the input is in the correct form 
  if( argc < FIXED_CLI_INPUTS ) showHelp();
  
  //allocate memory for the client
  this = secureAllocate(sizeof(client));
  if(this == NULL){
   printf("Error: Failed to instantiate a client\n");
   return NULL; 
  }
 
  //initialize the client properties
  this->router           = newRouter();
  this->torBindAddress   = argv[1];
  this->torPort          = argv[2];
  this->onionAddress     = argv[3];
  this->onionPort        = argv[4];
  this->operation        = argv[5]; 
  this->dirPath          = argv[6]; 
  
  //initialize the client public methods
  this->getFiles         = &getFiles;
  this->executeOperation = &executeOperation;

  //make sure the client is in a sane state
  if( !sanityCheck(argc, this->torBindAddress, this->onionPort, this->onionAddress) ){
    printf("Error: Failed to instantiate a client\n");
    return NULL; 
  }
  
  
  //prepare the file request string
  if( !prepareFileRequestString(this, argc, argv) ){
    printf("Error: Failed to instantiate a client\n");
    return NULL; 
  }
     
     
  //connect client to the hidden service
  if( !establishConnection(this) ){
    printf("Error: Failed to instantiate a client\n");
    return NULL; 
  }
  
  return this; 
}





/************ PUBLIC METHODS ******************/

//return 0 on error and 1 on success
static int executeOperation(client* this, int argc, char* argv[])
{
  if( this == NULL ){
    return 0; 
  }
  
  if  (   !strcmp(this->operation, "--get")  ) return this->getFiles(this, argc, argv);
  else                                         return showHelp();  
}



static int getFiles(client* this, int argc, char *argv[])
{
  int           currentFilePosition;
  int           filesRequested; 
  uint32_t      incomingFileBytesize;
  diskFile      *diskFile;
  dataContainer *incomingFile; 
  
  if( this == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  if( !this->router->transmitBytesize(this->router, this->fileRequestString->bytesize) ){
    printf("Error: Failed to transmit request string\n");
    return 0;
  }
  
  if( !this->router->transmit(this->router, this->fileRequestString->data, this->fileRequestString->bytesize) ){
    printf("Error: Failed to transmit request string\n");
    return 0;
  }
  
  
  for(currentFilePosition = FIRST_FILE_POSITION, filesRequested = argc - FIRST_FILE_POSITION; filesRequested--; currentFilePosition++){ 
    
    
    incomingFileBytesize = this->router->getIncomingBytesize(this->router); 
    if(incomingFileBytesize == -1){
      printf("Error: Failed to get incoming file bytesize, aborting\n");
      return 0;
    }
     
    incomingFile = this->router->receive(this->router, incomingFileBytesize);
    if(incomingFile == NULL){
      printf("Error: Failed to get incoming file, aborting\n");
      return 0; 
    }
    
    diskFile = newDiskFile(this->dirPath, argv[currentFilePosition], "w");
    if(diskFile == NULL){
      printf("Error: Failed to create file on disk, aborting\n");
      return 0;
    }
    
    if( !diskFile->diskFileWrite(diskFile, incomingFile) ){
      printf("Error: Failed to write file to disk, aborting\n");
      return 0;
    }
    
    if( !incomingFile->destroyDataContainer(&incomingFile) ){
      printf("Error: Failed to destroy data container\n");
      return 0; 
    }
    
    if( !diskFile->closeTearDown(&diskFile) ){
      printf("Error: Failed to tear down disk file\n");
      return 0;
    }
    
  }
  
  return 1; 
}



/************* PRIVATE METHODS ****************/ 
static uint32_t calculateFileRequestStringBytesize(int argc, char* argv[])
{
  uint32_t fileRequestStringBytesize; 
  uint32_t filesRequested;
  uint32_t currentFile;
  
  filesRequested = argc - FIRST_FILE_POSITION;
  currentFile    = FIRST_FILE_POSITION;
  
  for( fileRequestStringBytesize = 0 ; filesRequested-- ; currentFile++ ){
    fileRequestStringBytesize += strlen(argv[currentFile]) + DELIMITER_BYTESIZE;
  }
  
  return fileRequestStringBytesize;
}

//returns 0 on error
static int prepareFileRequestString(client* this, int argc, char* argv[])
{
  uint32_t       fileRequestStringBytesize; 
  uint32_t       filesRequested;
  uint32_t       currentFile; 
  uint32_t       currentWriteLocation; 
  uint32_t       sectionBytesize; 
  
  if(this == NULL){
    printf("Error: something was NULL that shouldn't have been\n"); 
    return 0;
  }
  
  filesRequested            = argc - FIRST_FILE_POSITION;
  currentFile               = FIRST_FILE_POSITION;
  
  fileRequestStringBytesize = calculateFileRequestStringBytesize(argc, argv); 
  
  this->fileRequestString = newDataContainer(fileRequestStringBytesize); 
  if(this->fileRequestString == NULL){
    printf("Error: Failed to allocate memory for file request string\n");
    return 0; 
  }
  
  
  for(currentWriteLocation = 0; filesRequested-- ; currentFile++){ 
    sectionBytesize = strlen(argv[currentFile]);
    memcpy( &(this->fileRequestString->data[currentWriteLocation]), argv[currentFile], sectionBytesize);
    currentWriteLocation += sectionBytesize;
    memcpy( &(this->fileRequestString->data[currentWriteLocation]), "/", DELIMITER_BYTESIZE);
    currentWriteLocation += DELIMITER_BYTESIZE; 
  }
  //string ends with NULL, this is enforced at server end as well 
  memcpy( &(this->fileRequestString->data[currentWriteLocation - 1]), "\0", 1);
  
  return 1; 
}


static int establishConnection(client* this)
{
 
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been"); 
    return 0;
  }
  
  if(!this->router->ipv4Connect(this->router, this->torBindAddress, this->torPort)){
    printf("Error: Failed to connect client\n");
    return 0;
  }
  
  if( !this->router->socks5Connect(this->router, this->onionAddress, ONION_ADDRESS_BYTESIZE, (uint16_t)strtol(this->onionPort, NULL, 10)) ){
    printf("Error: Failed to connect to destination address\n");
    return 0;
  }
  
  return 1; 
}

//returns 0 on error
static int sanityCheck(int argc, char* secondArgument, char* onionPort, char* onionAddress)
{  
  
  if( secondArgument == NULL || onionPort == NULL || onionAddress == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
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



static int showHelp()
{
    printf("\n\nSyntax: ./OnionGet [tor bind address] [tor listen port] [server onion address] [server onion port] [operation]\n");  
    printf("\nOperations\n");
    printf("\n--get [save path] [file names] --- gets each named file\n\n");
    return 1; 
}