#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include  <unistd.h>

#include "router.h"
#include "dataContainer.h"
#include "memoryManager.h"
#include "diskFile.h" 
#include "client.h"
#include "ogEnums.h" 

//PUBLIC METHODS
static int        executeOperation(clientObject *this);
static int        getFiles(clientObject *this);

//PRIVATE METHODS
static int        establishConnection(clientObject *this);
static uint32_t   calculateTotalRequestBytesize(clientObject *this);
static int        sendRequestedFilenames(clientObject *this);
static int        getIncomingFile(clientObject *this, diskFileObject *diskFile);



/************ OBJECT CONSTRUCTOR ******************/

/*    
 * newClient returns a client object on success and NULL on failure, the client object is already connected to onionAddress on onionPort, and the file request string
 * is already constructed.  
 * 
 */
clientObject *newClient(char *torBindAddress, char *torPort, char *onionAddress, char *onionPort, char *operation, char *dirPath, char **fileNames, uint32_t fileCount)
{
  clientObject *this; 
  
  if(torBindAddress == NULL || torPort == NULL || onionAddress == NULL || onionPort == NULL || operation == NULL || dirPath == NULL || *fileNames == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL; 
  }

  //allocate memory for the client object
  this = (clientObject *)secureAllocate(sizeof(*this));
  if(this == NULL){
   printf("Error: Failed to instantiate a client\n");
   return NULL; 
  }
 
  //initialize the client properties
  this->router = newRouter();
  if(this->router == NULL){
    printf("Error: Failed to create router for client, client instantiation failed\n");
    return NULL; 
  }
  
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

      
  //connect client to the hidden service
  if( !establishConnection(this) ){
    printf("Error: Failed to instantiate a client\n");
    return NULL; 
  }
  

  return this; 
}





/************ PUBLIC METHODS ******************/


/*
 * executeOperation returns 0 on error and 1 on success, this decides what to do based on the operation property of the client object
 * right now the only possible operation is --get which gets files from the server
 */
static int executeOperation(clientObject *this)
{
  if( this == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }
  
  //carry out the operation, if the operation isn't valid return 0 because it's an error (this is redundantly checked, also in controller.c) 
  if  (   !strcmp(this->operation, "--get")  ) return this->getFiles(this);
  else                                         return 0;  
}


/*
 * getFiles returns 0 on error and 1 on success, it sends the server the file request string, then goes through each file name 
 * and writes it to the disk with the data received from the server for that file name
 */
static int getFiles(clientObject *this)
{
  int            currentFile;
  uint32_t       fileCount; 
  diskFileObject *diskFile; 
  
  if( this == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  //send the server the request string
  if( !sendRequestedFilenames(this) ){
    printf("Error: Failed to send server request string\n");
    return 0;
  }
    
  //get the files from the server
  for(fileCount = this->fileCount, currentFile = 0; fileCount--; currentFile++){ 
    
    //first we open a new diskFile to write the file to
    diskFile = newDiskFile();
    if(diskFile == NULL){
      printf("Error: Failed to create new diskFile object\n");
      return 0;
    }
    
    if( !diskFile->dfOpen(diskFile, this->dirPath, this->fileNames[currentFile], "w") ){
      printf("Error: Failed to open file on disk\n");
      return 0; 
    }
    
    //then get the incoming file and write it to the disk
    if( !getIncomingFile(this, diskFile) ){
      printf("Error: Failed to get file\n");
      return 0; 
    }
    
    //then close the disk file
    if( !diskFile->closeTearDown(&diskFile) ){
      printf("Error: Failed to tear down disk file\n");
      return 0;
    }
 
  }
  
  //helps ensure that files are written to the disk (NOTE: sync after each individual file, or after all files like currently?)
  sync(); 
  
  return 1; 
}



/************* PRIVATE METHODS ****************/ 



/*
 * getIncomingFile returns 0 on error and 1 on success TODO check int types
 */ 
static int getIncomingFile(clientObject *this, diskFileObject *diskFile)
{
  dataContainerObject *incomingFileChunk   = NULL;
  uint32_t            incomingFileBytesize = 0; 
  uint64_t            bytesToGet           = 0; 
  uint32_t            bytesWritten         = 0; 
  uint32_t            writeOffset          = FILE_START; 
  
  if(this == NULL || diskFile == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }
  
  //the server sends us the incoming file's bytesize
  incomingFileBytesize = this->router->getIncomingBytesize(this->router); 
  if(!incomingFileBytesize){
    printf("Error: Failed to get incoming file bytesize, aborting\n");
    return 0;
  }
  
   
  //next we get the incoming file from the server in incomingFileChunkBytesize chunks, until there are no more chunks
  for(bytesToGet = 0 ; incomingFileBytesize ; incomingFileBytesize -=  bytesToGet){
    
    //if the remainder of the file is less than or equal to FILE_CHUNK_BYTESIZE then get all of it. Otherwise get FILE_CHUNK_BYTESIZE of it
    bytesToGet = (incomingFileBytesize <= FILE_CHUNK_BYTESIZE) ? incomingFileBytesize : FILE_CHUNK_BYTESIZE; 
        
    //get up to FILE_CHUNK_BYTESIZE bytes of the file
    incomingFileChunk = this->router->receive(this->router, bytesToGet);
    if(incomingFileChunk == NULL){
      printf("Error: Failed to receive data, aborting\n");
      return 0; 
    }
    
    //then write it to disk
    bytesWritten = diskFile->dfWrite(diskFile, incomingFileChunk, writeOffset);
    if(bytesWritten == 0){
      printf("Error: Failed to write file to disk, aborting\n");
      return 0;
    }
    writeOffset += bytesWritten; 
    
    
    //then destroy the data container that holds the current file chunk in RAM
    if( !incomingFileChunk->destroyDataContainer(&incomingFileChunk) ){
      printf("Error: Failed to destroy data container\n");
      return 0; 
    }
    
  }
  
  return 1; 
}


/*
 * sendRequestedFilenames returns 0 on error and 1 on success, it sends the already initialized file request string to the server
 */


//[request string bytesize][first filename bytesize][first file name][second filename bytesize][second file name]

static int sendRequestedFilenames(clientObject *this)
{
  uint32_t fileRequestStringBytesize;
  uint32_t currentFile;
  
  if( this == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  fileRequestStringBytesize = calculateTotalRequestBytesize(this);
  if(fileRequestStringBytesize == -1){
    printf("Error: failed to calculate file request string bytesize\n");
    return 0; 
  }
  
  //let the server know the bytesize of the request string
  if( !this->router->transmitBytesize(this->router, fileRequestStringBytesize) ){
    printf("Error: Failed to transmit request string\n");
    return 0;
  }
  
  //send the server the requested file file file names TODO clean this up (ie: don't take strlen twice)  
  for(currentFile = 0; currentFile != this->fileCount; currentFile++){
    if(!this->router->transmitBytesize(this->router, strlen(this->fileNames[currentFile])) ){
      printf("Error: Failed to send server file name bytesize\n");
      return 0; 
    }
    
    if(!this->router->transmit(this->router, this->fileNames[currentFile], strlen(this->fileNames[currentFile])) ){
      printf("Error: Failed to send server file name"); 
      return 0; 
    } 
  }
  
  
  return 1; 
}


/*
 * calculateTotalRequestBytesize returns -1 on error and the bytesize of the file request string on success
 */
static uint32_t calculateTotalRequestBytesize(clientObject *this)
{
  uint32_t fileRequestStringBytesize; 
  uint32_t currentFile;
  uint32_t fileCount;
  
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return -1; 
  }
  
  //get bytesize of each requested file name and add it to the file request strings total bytesize + 1 byte each for the delimiter
  for( fileCount = this->fileCount, currentFile = 0, fileRequestStringBytesize = 0 ; fileCount-- ; currentFile++ ){
    fileRequestStringBytesize += sizeof(uint32_t) + strlen(this->fileNames[currentFile]);
  }
  
  //return the fileRequestStringBytesize
  return fileRequestStringBytesize;
}


 

/*
 * establishConnection returns 0 on error and 1 on success, it connects the client object to the server 
 */
static int establishConnection(clientObject *this)
{
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been"); 
    return 0;
  }
  
  //connect the client objects router to Tor //TODO consider moving this to networking functions 
  if(!this->router->ipv4Connect(this->router, this->torBindAddress, this->torPort)){
    printf("Error: Failed to connect client\n");
    return 0;
  }
  
  //tell Tor to connect to the onion_address:port
  if( !this->router->socks5Connect(this->router, this->onionAddress, ONION_ADDRESS_BYTESIZE, (uint16_t)strtol(this->onionPort, NULL, 10)) ){
    printf("Error: Failed to connect to destination address\n");
    return 0;
  }
  
  return 1; 
}