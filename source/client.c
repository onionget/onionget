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



//private internal values 
typedef struct clientPrivate{
  clientObject   publicClient;
  routerObject  *router;
}clientPrivate;



//PUBLIC METHODS
static int   getFiles(clientObject *this, char *dirPath, char **fileNames, uint32_t fileCount);
static int   initializeSocks(clientObject *this, char *torBindAddress, char *torPort);
static int   establishConnection(clientObject *this, char *onionAddress, char *onionPort);
static int   setRouter(clientObject *client, routerObject *router);   


//PRIVATE METHODS
static uint32_t  calculateTotalRequestBytesize(char **fileNames, uint32_t fileCount);
static int       sendRequestedFilenames(clientObject *this, char **fileNames, uint32_t fileCount);
static int       getIncomingFile(clientObject *this, diskFileObject *diskFile);
static int       hsValueSanityCheck(char *onionAddress, char *onionPort);



/************ OBJECT CONSTRUCTOR ******************/

/*    
 * newClient returns a client object on success and NULL on failure
 * 
 */
clientObject *newClient(void)
{
  clientPrivate *privateThis = NULL;
  
  //Allocate memory for the object
  privateThis = (clientPrivate *)secureAllocate(sizeof(*privateThis)); 
  if(privateThis == NULL){
    printf("Error: Failed to allocate new client object\n");
    return NULL; 
  }
  
  //initialize public methods
  privateThis->publicClient.getFiles            = &getFiles; 
  privateThis->publicClient.establishConnection = &establishConnection; 
  privateThis->publicClient.initializeSocks     = &initializeSocks; 
  privateThis->publicClient.setRouter           = &setRouter; 
  
  //initialize private properties
  privateThis->router = NULL;


  return (clientObject*)privateThis; 
}



/************ PUBLIC METHODS ******************/


//returns 0 on error and 1 on success (dependency injection) 
static int setRouter(clientObject *this, routerObject *router)
{
  clientPrivate *private = NULL;
  
  private = (clientPrivate *)this;
  
  if(private == NULL || this == NULL || router == NULL){
    printf("Error: Failed to set the clients router\n");
    return 0; 
  }
  
  private->router = router;
  
  return 1; 
}


//TODO sanity check these values
static int initializeSocks(clientObject *this, char *torBindAddress, char *torPort)
{
  clientPrivate *private = NULL;
  
  private = (clientPrivate *)this;
  
  if(this == NULL || private == NULL || private->router == NULL || torBindAddress == NULL || torPort == NULL){
    printf("Error: something was NULL that shouldn't have been\n");
    return 0; 
  }
  
  //connect the client objects router to Tor 
  if(!private->router->ipv4Connect(private->router, torBindAddress, torPort)){
    printf("Error: Failed to connect client\n");
    return 0;
  }
  
  return 1; 
}


/*
 * establishConnection returns 0 on error and 1 on success
 */
static int establishConnection(clientObject *this, char *onionAddress, char *onionPort)
{
  clientPrivate *private = NULL;
  
  private = (clientPrivate *)this; 
  
  //sanity checking
  if(private == NULL || private->router == NULL || this == NULL || onionAddress == NULL || onionPort == NULL){
    printf("Error: Something was NULL that shouldn't have been\n"); 
    return 0;
  }
  
  if( !hsValueSanityCheck(onionAddress, onionPort) ){
    printf("Error: Invalid onion destination\n");
    return 0; 
  }
     
  //tell Tor to connect to the onion_address:port
  if( !private->router->socks5Connect(private->router, onionAddress, ONION_ADDRESS_BYTESIZE, (uint16_t)strtol(onionPort, NULL, 10)) ){
    printf("Error: Failed to connect to destination address\n");
    return 0;
  }
  
  return 1; 
}





/*
 * getFiles returns 0 on error and 1 on success, it sends the server the file request string, then goes through each file name 
 * and writes it to the disk with the data received from the server for that file name TODO dependency inject diskFile? TODO sanity check dirpath 
 */
static int getFiles(clientObject *this, char *dirPath, char **fileNames, uint32_t fileCount)
{
  int            currentFile = 0;
  diskFileObject *diskFile   = NULL; 
  
  if( this == NULL || dirPath == NULL || fileNames == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  //send the server the requested file names
  if( !sendRequestedFilenames(this, fileNames, fileCount) ){
    printf("Error: Failed to send server request string\n");
    return 0;
  }
    
  //get the files from the server
  for(currentFile = 0; fileCount--; currentFile++){ 
    
    //first we open a new diskFile to write the file to
    diskFile = newDiskFile();
    if(diskFile == NULL){
      printf("Error: Failed to create new diskFile object\n");
      return 0;
    }
    
    if( !diskFile->dfOpen(diskFile, dirPath, fileNames[currentFile], "w") ){
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
  
  //helps ensure that files are written to the disk (TODO: sync after each individual file, or after all files like currently, or in fileDisk, ????)
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
  
  clientPrivate *private = NULL;
  private = (clientPrivate *)this; 
  
  if(private == NULL || private->router == NULL || this == NULL || diskFile == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }
  
  //the server sends us the incoming file's bytesize
  incomingFileBytesize = private->router->getIncomingBytesize(private->router); 
  if(!incomingFileBytesize){
    printf("Error: Failed to get incoming file bytesize, aborting\n");
    return 0;
  }
  
   
  //next we get the incoming file from the server in incomingFileChunkBytesize chunks, until there are no more chunks
  for(bytesToGet = 0 ; incomingFileBytesize ; incomingFileBytesize -=  bytesToGet){
    
    //if the remainder of the file is less than or equal to FILE_CHUNK_BYTESIZE then get all of it. Otherwise get FILE_CHUNK_BYTESIZE of it
    bytesToGet = (incomingFileBytesize <= FILE_CHUNK_BYTESIZE) ? incomingFileBytesize : FILE_CHUNK_BYTESIZE; 
        
    //get up to FILE_CHUNK_BYTESIZE bytes of the file
    incomingFileChunk = private->router->receive(private->router, bytesToGet);
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
 * sendRequestedFilenames returns 0 on error and 1 on success
 * 
 * [request string bytesize][first filename bytesize][first file name][second filename bytesize][second file name]
 */
static int sendRequestedFilenames(clientObject *this, char **fileNames, uint32_t fileCount)
{
  uint32_t      fileRequestStringBytesize = 0;
  uint32_t      currentFile               = 0;
  clientPrivate *private                  = NULL;
  
  private = (clientPrivate *)this;
  
  if( private == NULL || private->router == NULL || this == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  fileRequestStringBytesize = calculateTotalRequestBytesize(fileNames, fileCount);
  if(fileRequestStringBytesize == -1){
    printf("Error: failed to calculate file request string bytesize\n");
    return 0; 
  }
  
  //let the server know the bytesize of the request string
  if( !private->router->transmitBytesize(private->router, fileRequestStringBytesize) ){
    printf("Error: Failed to transmit request string\n");
    return 0;
  }
  
  //send the server the requested file file file names TODO clean this up (ie: don't take strlen twice)  
  for(currentFile = 0; currentFile != fileCount; currentFile++){
    if(!private->router->transmitBytesize(private->router, strlen(fileNames[currentFile])) ){
      printf("Error: Failed to send server file name bytesize\n");
      return 0; 
    }
    
    if(!private->router->transmit(private->router, fileNames[currentFile], strlen(fileNames[currentFile])) ){
      printf("Error: Failed to send server file name"); 
      return 0; 
    } 
  }
  
  
  return 1; 
}


/*
 * calculateTotalRequestBytesize returns 0 on error and the bytesize of the file request string on success //TODO set errno or something?
 */
static uint32_t calculateTotalRequestBytesize(char **fileNames, uint32_t fileCount)
{
  uint32_t fileRequestStringBytesize = 0; 
  uint32_t currentFile               = 0;

  
  if(*fileNames == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }
  
  //get bytesize of each requested file name and add it to the file request strings total bytesize + 1 byte each for the delimiter
  for(currentFile = 0, fileRequestStringBytesize = 0 ; fileCount-- ; currentFile++ ){
    fileRequestStringBytesize += sizeof(uint32_t) + strlen(fileNames[currentFile]);
  }
  
  //return the fileRequestStringBytesize
  return fileRequestStringBytesize;
}



static int hsValueSanityCheck(char *onionAddress, char *onionPort)
{  
  if( onionPort == NULL || onionAddress == NULL){
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

 

