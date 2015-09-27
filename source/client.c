#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include  <unistd.h>

#include "router.h"
#include "memoryManager.h"
#include "diskFile.h" 
#include "client.h"
#include "ogEnums.h" 
#include "macros.h"



//private internal values 
typedef struct clientPrivate{
  clientObject   publicClient;
  routerObject  *router;
}clientPrivate;



//PUBLIC METHODS
static int   getFiles(clientObject *this, char *dirPath, char **fileNames, uint32_t fileCount, diskFileObject *clientFileInterface);
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
clientObject *newClient(routerObject *router)
{
  clientPrivate *privateThis = NULL;
  
  //Allocate memory for the object
  privateThis = (clientPrivate *)secureAllocate(sizeof(*privateThis)); 
  if(privateThis == NULL){
    logEvent("Error", "Failed to allocate new client object");
    return NULL; 
  }
  
  //initialize public methods
  privateThis->publicClient.getFiles            = &getFiles; 
  privateThis->publicClient.establishConnection = &establishConnection; 
  privateThis->publicClient.initializeSocks     = &initializeSocks; 
  
  //initialize private properties
  privateThis->router = router;


  return (clientObject*)privateThis; 
}



/************ PUBLIC METHODS ******************/



//TODO sanity check these values
static int initializeSocks(clientObject *this, char *torBindAddress, char *torPort)
{
  clientPrivate *private = NULL;
  
  private = (clientPrivate *)this;
  
  if(this == NULL || private == NULL || private->router == NULL || torBindAddress == NULL || torPort == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0; 
  }
  
  //connect the client objects router to Tor 
  if(!private->router->ipv4Connect(private->router, torBindAddress, torPort)){
    logEvent("Error", "Failed to connect client");
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
    logEvent("Error", "Something was NULL that shouldn't have been"); 
    return 0;
  }
  
  if( !hsValueSanityCheck(onionAddress, onionPort) ){
    logEvent("Error", "Invalid onion destination");
    return 0; 
  }
     
  //tell Tor to connect to the onion_address:port
  if( !private->router->socks5Connect(private->router, onionAddress, ONION_ADDRESS_BYTESIZE, (uint16_t)strtol(onionPort, NULL, 10)) ){
    logEvent("Error", "Failed to connect to destination address");
    return 0;
  }
  
  return 1; 
}





/*
 * getFiles returns 0 on error and 1 on success, it sends the server the file request string, then goes through each file name 
 * and writes it to the disk with the data received from the server for that file name TODO dependency inject diskFile? TODO sanity check dirpath 
 */
static int getFiles(clientObject *this, char *dirPath, char **fileNames, uint32_t fileCount, diskFileObject *clientFileInterface)
{
  int currentFile = 0;
  
  if( this == NULL || dirPath == NULL || fileNames == NULL || clientFileInterface == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  //send the server the requested file names
  if( !sendRequestedFilenames(this, fileNames, fileCount) ){
    logEvent("Error", "Failed to send server request string");
    return 0;
  }
    
  //get the files from the server
  for(currentFile = 0; fileCount--; currentFile++){ 
    
    if( !clientFileInterface->dfOpen(clientFileInterface, dirPath, fileNames[currentFile], "w") ){
      logEvent("Error", "Failed to open file on disk");
      return 0; 
    }
    
    //then get the incoming file and write it to the disk
    if( !getIncomingFile(this, clientFileInterface) ){
      logEvent("Error", "Failed to get file");
      return 0; 
    }
    
    //then reinitialize the clientFileInterface
    if( !clientFileInterace->reinitialize(clientFileInterface) ){ //TODO TODO TODO TODO TODO TODO TODO need to implement this
      logEvent("Error", "Failed to tear down disk file");
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
  char                incomingFileChunk[FILE_CHUNK_BYTESIZE]; 
  
  uint32_t            incomingFileBytesize = 0; 
  size_t              bytesToGet           = 0; 
  uint32_t            bytesWritten         = 0; 
  uint32_t            writeOffset          = FILE_START; 
  
  clientPrivate *private = NULL;
  private = (clientPrivate *)this; 
  
  if(private == NULL || private->router == NULL || this == NULL || diskFile == NULL){
    memoryClear(incomingFileChunk, FILE_CHUNK_BYTESIZE);
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0; 
  }
  
  //the server sends us the incoming file's bytesize
  incomingFileBytesize = private->router->getIncomingBytesize(private->router); 
  if(!incomingFileBytesize){
    memoryClear(incomingFileChunk, FILE_CHUNK_BYTESIZE);
    logEvent("Error", "Failed to get incoming file bytesize, aborting");
    return 0;
  }
  
   
  //next we get the incoming file from the server in incomingFileChunkBytesize chunks, until there are no more chunks
  for(bytesToGet = 0 ; incomingFileBytesize ; incomingFileBytesize -=  bytesToGet){
    
    //if the remainder of the file is less than or equal to FILE_CHUNK_BYTESIZE then get all of it. Otherwise get FILE_CHUNK_BYTESIZE of it
    bytesToGet = (incomingFileBytesize <= FILE_CHUNK_BYTESIZE) ? incomingFileBytesize : FILE_CHUNK_BYTESIZE; 
        
    //get up to FILE_CHUNK_BYTESIZE bytes of the file
    if( !private->router->receive(private->router, incomingFileChunk, bytesToGet) ){
      memoryClear(incomingFileChunk, FILE_CHUNK_BYTESIZE);
      logEvent("Error", "Failed to receive data chunk");
      return 0; // TODO good error checking soon (plus wipe)
    }
           
    //then write it to disk
    bytesWritten = diskFile->dfWrite(diskFile, incomingFileChunk, bytesToGet, writeOffset);
    if(bytesWritten == 0){
      memoryClear(incomingFileChunk, FILE_CHUNK_BYTESIZE);
      logEvent("Error", "Failed to write file to disk, aborting");
      return 0;
    }
    writeOffset += bytesWritten; 

  }
  
  memoryClear(incomingFileChunk, FILE_CHUNK_BYTESIZE);
  
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
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  fileRequestStringBytesize = calculateTotalRequestBytesize(fileNames, fileCount);
  if(fileRequestStringBytesize == -1){
    logEvent("Error", "Failed to calculate file request string bytesize");
    return 0; 
  }
  
  //let the server know the bytesize of the request string
  if( !private->router->transmitBytesize(private->router, fileRequestStringBytesize) ){
    logEvent("Error", "Failed to transmit request string");
    return 0;
  }
  
  //send the server the requested file file file names TODO clean this up (ie: don't take strlen twice)  
  for(currentFile = 0; currentFile != fileCount; currentFile++){
    if(!private->router->transmitBytesize(private->router, strlen(fileNames[currentFile])) ){
      logEvent("Error", "Failed to send server file name bytesize");
      return 0; 
    }
    
    if(!private->router->transmit(private->router, fileNames[currentFile], strlen(fileNames[currentFile])) ){
      logEvent("Error", "Failed to send server file name"); 
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
    logEvent("Error", "Something was NULL that shouldn't have been");
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
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }

  if( strlen(onionPort) > 5 || strtol(onionPort, NULL, 10) > 65535){
   logEvent("Error", "Port of destination must be at or below 65535");
   return 0; 
  }
  
  if( strlen(onionAddress) != ONION_ADDRESS_BYTESIZE ){
   logEvent("Error", "Put onion address in form abcdefghijklmnop.onion");
   return 0; 
  }
  
  return 1; 
}