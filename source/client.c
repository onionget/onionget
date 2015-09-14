#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "router.h"
#include "datacontainer.h"
#include "memorymanager.h"
#include "diskfile.h" 
#include "client.h"
#include "og_enums.h" 

//PUBLIC METHODS
static int            executeOperation(client *this);
static int            getFiles(client *this);

//PRIVATE METHODS
static int            establishConnection(client *this);
static int            prepareFileRequestString(client *this);
static uint32_t       calculateFileRequestStringBytesize(client *this);
static int            sendRequestString(client *this);
static dataContainer  *getIncomingFile(client *this);
static int writeFileToDisk(client *this, char *fileName, dataContainer *fileData);



/*                                                                   CLIENT OBJECT CONSTRUCTOR
 * 
 * newClient returns a client object on success and NULL on failure, the client object is already connected to onionAddress on onionPort, and the file request string
 * is already constructed.  
 * 
 */
client *newClient(char *torBindAddress, char *torPort, char *onionAddress, char *onionPort, char *operation, char *dirPath, char **fileNames, uint32_t fileCount)
{
  client *this; 
  
  if(torBindAddress == NULL || torPort == NULL || onionAddress == NULL || onionPort == NULL || operation == NULL || dirPath == NULL || *fileNames == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL; 
  }

  //allocate memory for the client object
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


/*
 * executeOperation returns 0 on error and 1 on success, this decides what to do based on the operation property of the client object
 * right now the only possible operation is --get which gets files from the server
 */
static int executeOperation(client *this)
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
static int getFiles(client *this)
{
  int           currentFile;
  uint32_t      fileCount; 
  dataContainer *incomingFile; 
  
  if( this == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  //send the server the request string
  if( !sendRequestString(this) ){
    printf("Error: Failed to send server request string\n");
    return 0;
  }
    
  //get the files from the server
  for(fileCount = this->fileCount, currentFile = 0; fileCount--; currentFile++){ 

    //first get the incoming file
    incomingFile = getIncomingFile(this);
    if(incomingFile == NULL){
      printf("Error: Failed to receive incoming file\n");
      return 0; 
    }
    
    //then write it to the disk
    if( !writeFileToDisk(this, this->fileNames[currentFile], incomingFile) ){
      printf("Error: Failed to write file to disk\n");
      return 0;
    }
    
    //then destroy the data container that holds the current file in RAM
    if( !incomingFile->destroyDataContainer(&incomingFile) ){
      printf("Error: Failed to destroy data container\n");
      return 0; 
    }
    
  }
  
  return 1; 
}



/************* PRIVATE METHODS ****************/ 

/*
 * writeFileToDisk returns 0 on error and 1 on success
 * it writes the current file to the disk 
 */
static int writeFileToDisk(client *this, char *fileName, dataContainer *fileData)
{
  diskFile *diskFile;
  
  if(this == NULL || fileName == NULL || fileData == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }
  
  //we instantiate a new diskFile object to write the received file to the disk
  diskFile = newDiskFile(this->dirPath, fileName, "w");
  if(diskFile == NULL){
    printf("Error: Failed to create file on disk, aborting\n");
    return 0;
  }
    
  //then we write the received file to the disk
  if( !diskFile->diskFileWrite(diskFile, fileData) ){
    printf("Error: Failed to write file to disk, aborting\n");
    return 0;
  }
    
  //then we close the disk file and destroy the object
  if( !diskFile->closeTearDown(&diskFile) ){
    printf("Error: Failed to tear down disk file\n");
    return 0;
  }
  
  return 1; 
}

/*
 * getIncomingFile returns NULL on error and a dataContainer with the received file on success
 */
static dataContainer *getIncomingFile(client *this)
{
  dataContainer *incomingFile;
  uint32_t      incomingFileBytesize;
  
  
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL; 
  }

  //first the server sends us the incoming files bytesize
  incomingFileBytesize = this->router->getIncomingBytesize(this->router); 
  if(incomingFileBytesize == -1){
    printf("Error: Failed to get incoming file bytesize, aborting\n");
    return NULL;
  }
  
  //next we get the incoming file from the server
  incomingFile = this->router->receive(this->router, incomingFileBytesize);
  if(incomingFile == NULL){
    printf("Error: Failed to receive data, aborting\n");
    return NULL; 
  }
  
  //then we return the incoming file
  return incomingFile; 
}


/*
 * sendRequestString returns 0 on error and 1 on success, it sends the already initialized file request string to the server
 */
static int sendRequestString(client *this)
{
  if( this == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  //let the server know the bytesize of the request string
  if( !this->router->transmitBytesize(this->router, this->fileRequestString->bytesize) ){
    printf("Error: Failed to transmit request string\n");
    return 0;
  }
  
  //then send the server the request string
  if( !this->router->transmit(this->router, this->fileRequestString->data, this->fileRequestString->bytesize) ){
    printf("Error: Failed to transmit request string\n");
    return 0;
  }
  
  return 1; 
}


/*
 * calculateFileRequestStringBytesize returns -1 on error and the bytesize of the file request string on success
 */
static uint32_t calculateFileRequestStringBytesize(client *this)
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
    fileRequestStringBytesize += strlen(this->fileNames[currentFile]) + DELIMITER_BYTESIZE;
  }
  
  //return the fileRequestStringBytesize
  return fileRequestStringBytesize;
}

/*
 * prepareFileRequestString returns 0 on error and 1 on success. It initializes the file request string property of the client object.  
 */
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
  
  //calculate the bytesize of the file request string
  fileRequestStringBytesize = calculateFileRequestStringBytesize(this); 
  if(fileRequestStringBytesize == -1){
    printf("Error: Failed to compute file request string bytesize\n");
    return 0;
  }
  
  //make a dataContainer object for holding the file request string
  this->fileRequestString = newDataContainer(fileRequestStringBytesize); 
  if(this->fileRequestString == NULL){
    printf("Error: Failed to allocate memory for file request string\n");
    return 0; 
  }
  
  //go through the file names and concatenate them together separated by the '/' delimiter
  //preformat: "fileName/AnotherName/yetAnother/"
  for(fileCount = this->fileCount, currentFile = 0, currentWriteLocation = 0; fileCount-- ; currentFile++){ 
    sectionBytesize = strlen(this->fileNames[currentFile]);
    memcpy( &(this->fileRequestString->data[currentWriteLocation]), this->fileNames[currentFile], sectionBytesize);
    currentWriteLocation += sectionBytesize;
    memcpy( &(this->fileRequestString->data[currentWriteLocation]), "/", DELIMITER_BYTESIZE);
    currentWriteLocation += DELIMITER_BYTESIZE; 
  }
  
  //however, the string must be null terminated, this is enforced at the server end, but we set it here as well 
  //final format: "fileName/AnotherName/yetAnother\0" 
  memcpy( &(this->fileRequestString->data[currentWriteLocation - 1]), "\0", 1);
  
  return 1; 
}

/*
 * establishConnection returns 0 on error and 1 on success, it connects the client object to the server 
 */
static int establishConnection(client *this)
{
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been"); 
    return 0;
  }
  
  //connect the client objects router to Tor 
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