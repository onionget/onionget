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
enum{  ONION_ADDRESS_BYTESIZE = 22  };

static int establishConnection(client *this);
static int prepareFileRequestString(client *this);
static uint32_t calculateFileRequestStringBytesize(client *this);
static int getFiles(client *this);
static int executeOperation(client *this);


/******** CONSTRUCTOR ***********/
client *newClient(char *torBindAddress, char *torPort, char *onionAddress, char *onionPort, char *operation, char *dirPath, char **fileNames, uint32_t fileCount)
{
  client *this; 
  
  if(torBindAddress == NULL || torPort == NULL || onionAddress == NULL || onionPort == NULL || operation == NULL || dirPath == NULL || *fileNames == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL; 
  }

  //allocate memory for the client
  this = secureAllocate(sizeof(struct client));
  if(this == NULL){
   printf("Error: Failed to instantiate a client\n");
   return NULL; 
  }
 
  //initialize the client properties
  this->router           = newRouter();
  this->torBindAddress   = torBindAddress;
  this->torPort          = torPort;
  this->onionAddress     = onionAddress;
  this->onionPort        = onionPort;
  this->operation        = operation;
  this->dirPath          = dirPath;  
  this->fileNames        = fileNames; 
  this->fileCount        = fileCount; 
  
  
  //initialize the client public methods
  this->getFiles         = &getFiles;
  this->executeOperation = &executeOperation;

  
  //prepare the file request string
  if( !prepareFileRequestString(this) ){
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
static int executeOperation(client *this)
{
  if( this == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }
  
  if  (   !strcmp(this->operation, "--get")  ) return this->getFiles(this);
  else                                         return 0;  
}



static int getFiles(client *this)
{
  int           currentFilePosition;
  uint32_t      incomingFileBytesize;
  uint32_t      fileCount; 
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
  
  
  for(fileCount = this->fileCount, currentFilePosition = 0; fileCount--; currentFilePosition++){ 
    
    
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
    
    diskFile = newDiskFile(this->dirPath, this->fileNames[currentFilePosition], "w");
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

//returns -1 on error
static uint32_t calculateFileRequestStringBytesize(client *this)
{
  uint32_t fileRequestStringBytesize; 
  uint32_t currentFile;
  uint32_t fileCount;
  
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return -1; 
  }
  
  for( fileCount = this->fileCount, currentFile = 0, fileRequestStringBytesize = 0 ; fileCount-- ; currentFile++ ){
    fileRequestStringBytesize += strlen(this->fileNames[currentFile]) + DELIMITER_BYTESIZE;
  }
  
  return fileRequestStringBytesize;
}

//returns 0 on error
static int prepareFileRequestString(client *this)
{
  uint32_t   fileRequestStringBytesize; 
  uint32_t   currentFile; 
  uint32_t   currentWriteLocation; 
  uint32_t   sectionBytesize; 
  uint32_t   fileCount;
  
  if(this == NULL){
    printf("Error: something was NULL that shouldn't have been\n"); 
    return 0;
  }
  
  fileRequestStringBytesize = calculateFileRequestStringBytesize(this); 
  if(fileRequestStringBytesize == -1){
    printf("Error: Failed to compute file request string bytesize\n");
    return 0;
  }
  
  this->fileRequestString = newDataContainer(fileRequestStringBytesize); 
  if(this->fileRequestString == NULL){
    printf("Error: Failed to allocate memory for file request string\n");
    return 0; 
  }
  
  
  for(fileCount = this->fileCount, currentFile = 0, currentWriteLocation = 0; fileCount-- ; currentFile++){ 
    sectionBytesize = strlen(this->fileNames[currentFile]);
    memcpy( &(this->fileRequestString->data[currentWriteLocation]), this->fileNames[currentFile], sectionBytesize);
    currentWriteLocation += sectionBytesize;
    memcpy( &(this->fileRequestString->data[currentWriteLocation]), "/", DELIMITER_BYTESIZE);
    currentWriteLocation += DELIMITER_BYTESIZE; 
  }
  
  //string ends with NULL, this must be enforced at server end as well 
  memcpy( &(this->fileRequestString->data[currentWriteLocation - 1]), "\0", 1);
  
  return 1; 
}


static int establishConnection(client *this)
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