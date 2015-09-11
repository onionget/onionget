#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "datacontainer.h"
#include "diskfile.h"
#include "memorymanager.h"


enum{ COUNT = 1 };

//PUBLIC METHODS
static int diskFileWrite(diskFile* this, dataContainer* dataContainer);
static dataContainer* diskFileRead(diskFile* this);


static void closeTearDown(diskFile** thisPointer);
static long int getBytesize(diskFile* this);


//PRIVATE METHODS
static int diskFileOpen(diskFile* this, char* mode);
static int fileModeReadable(char* mode);
static int fileModeWritable(char* mode);
static int fileModeValid(char* mode);
static int fileModeSeekable(char* mode); 


//returns NULL on error
diskFile* newDiskFile(char* path, char* name, char* mode)
{
  diskFile  *this;
  uint32_t  pathBytesize;
  uint32_t  nameBytesize;
  uint32_t fullPathBytesize; 
  
  //sanity check
  if( !fileModeValid(mode) ){
    printf("Error: Invalid file mode specified\n");
    return NULL;
  }
  
  //get string sizes
  pathBytesize         = strlen(path);
  nameBytesize         = strlen(name);
  fullPathBytesize     = pathBytesize + nameBytesize;
 
  //allocate memory for the object
  this = secureAllocate(sizeof(struct diskFile)); 
  if(this == NULL){
    printf("Error: Failed to allocate memory for disk file\n");
    return NULL; 
  }
  
  
  //create the full path to the file, adding a terminating / if required
  if( path[pathBytesize - 1] != '/' ){
    this->fullPathBytesize = fullPathBytesize + 2; 
    
    this->fullPath  = secureAllocate(this->fullPathBytesize); //must +1 to ensure null termination, another +1 to add terminating / 
    if(this->fullPath == NULL){
      printf("Error: Failed to allocate memory to hold path\n");
      secureFree(&this, sizeof(struct diskFile));
      return NULL; 
    }
    
    memcpy( &(this->fullPath[0]) , path , pathBytesize);
    memcpy(&(this->fullPath[pathBytesize]), "/", 1);
    memcpy(&(this->fullPath[pathBytesize + 1]), name, nameBytesize);
  }
  else{
    this->fullPathBytesize = fullPathBytesize + 1; 
    
    this->fullPath  = secureAllocate(this->fullPathBytesize); //must +1 to ensure null termination
    if(this->fullPath == NULL){
      printf("Error: Failed to allocate memory to hold path\n");
      secureFree(&this, sizeof(struct diskFile));
      return NULL; 
    }
    
    
    memcpy( &(this->fullPath[0]) , path , pathBytesize);
    memcpy( &(this->fullPath[pathBytesize]) , name , nameBytesize); 
  }
  
  
  //open the file in the specified mode
  if( diskFileOpen(this, mode) == -1 ){
    printf("Error: Failed to open file\n");
    secureFree(&this->fullPath, fullPathBytesize);
    secureFree(&this, sizeof(struct diskFile));
    return NULL;
  }
  

  //initialize public methods
  this->diskFileWrite         = &diskFileWrite;
  this->diskFileRead          = &diskFileRead; 
  this->closeTearDown = &closeTearDown;
  this->getBytesize   = &getBytesize; 
  

  //NOTE TODO open is my own and it is messy refactor this code
  
  return this; 
}




/******** PUBLIC METHODS *********/

//returns -1 on error
static long int getBytesize(diskFile* this)
{
  long int fileBytesize; 
  
  if(!fileModeSeekable(this->mode)){
    printf("Error: File is not in a mode that lets us get its bytesize\n");
    return -1; 
  }
  
  if( fseek(this->descriptor, 0, SEEK_END) == -1 ){
    printf("Error: Failed to seek to end of file\n");
    return -1; 
  }
  
  if( (fileBytesize = ftell(this->descriptor)) == -1 ){
    printf("Error: Failed to get file bytesize\n");
    return -1; 
  }
  
  if( fseek(this->descriptor, 0, 0) == -1 ){
    printf("Error: Failed to seek to start of file\n");
    return -1; 
  }
  
  return fileBytesize; 
}




//closes the file descriptor if it is open and destroys the struct (not named destroy because it doesn't destroy the file and might be confusing)
static void closeTearDown(diskFile** thisPointer)
{
  diskFile *this;
  
  this = *thisPointer; 
  
  if(this->descriptor != NULL){
    if( fclose(this->descriptor) == EOF ){
      printf("Error: Failed to close file descriptor\n"); 
    }
  }
    
  secureFree( &(this->fullPath), this->fullPathBytesize); 
  secureFree(thisPointer, sizeof(struct diskFile)); 
}

//returns 0 on error
static int diskFileWrite(diskFile* this, dataContainer* dataContainer) 
{
  if(!fileModeWritable(this->mode)){
    printf("Error: File is not in a writable mode\n");
    return 0;
  }
  
  if(!this->descriptor){
    printf("Error: File is not open");
    return 0;
  }
  
  //TODO look more at error checking
  if( fwrite( (const void*)dataContainer->data, dataContainer->bytesize, COUNT, this->descriptor) != COUNT ){
    printf("Error: didn't write all data to file\n");
    return 0; 
  }
  
  
  return 1; 
}


//returns NULL on error
static dataContainer* diskFileRead(diskFile* this)
{
  dataContainer *dataContainer; 
  long int      fileBytesize;
  
  if(!fileModeReadable(this->mode)){
    printf("Error: File is not in a readable mode\n");
    return NULL; 
  }
  
  if(!this->descriptor){
    printf("Error: File is not open\n");
    return NULL;
  }
  
  fileBytesize = this->getBytesize(this); 
  if(fileBytesize == -1){
    printf("Error: Failed to get file bytesize, cannot read it into data container\n");
    return NULL; 
  }
  
  dataContainer = newDataContainer(fileBytesize);
  if(dataContainer == NULL){
    printf("Error: Failed to create data container to read file into\n");
    return NULL; 
  }

  
  //TODO look into error checking more
  if( fread(dataContainer->data, fileBytesize, COUNT, this->descriptor) != COUNT){
    printf("Error: Failed to read file into data container\n");
    secureFree( &dataContainer->data, dataContainer->bytesize );
    secureFree( &dataContainer, sizeof(struct dataContainer)); 
    return NULL; 
  }
  

  return dataContainer; 
}




/******** PRIVATE METHODS *********/

//returns -1 on error
static int diskFileOpen(diskFile* this, char* mode)
{
  this->descriptor = fopen( (const char*)this->fullPath, mode );
  
  if(this->descriptor == NULL){
    printf("Error: Failed to open file\n"); 
    return -1; 
  }
  
  this->mode = mode;
  
  return 1;
}


static int fileModeValid(char* mode)
{
  if(  !memcmp(mode, "w", 1) | !memcmp(mode, "a", 1) | !memcmp(mode, "r+", 2) | !memcmp(mode, "w+", 2) | !memcmp(mode, "a+", 2) | !memcmp(mode, "r", 1) ){
    return 1; 
  } 
  return 0; 
}

static int fileModeWritable(char* mode)
{
  if(  !memcmp(mode, "w", 1) | !memcmp(mode, "a", 1) | !memcmp(mode, "r+", 2) | !memcmp(mode, "w+", 2) | !memcmp(mode, "a+", 2) ){
   return 1; 
  }
  return 0; 
}

static int fileModeSeekable(char* mode)
{
  if( !memcmp(mode, "w", 1) | !memcmp(mode, "r+", 2) | !memcmp(mode, "w+", 2) | !memcmp(mode, "r", 1) ){
    return 1;
  }
  return 0;
}

static int fileModeReadable(char* mode)
{
  if( !memcmp(mode, "r", 1) | !memcmp(mode, "r+", 2) | !memcmp(mode, "w+", 2) | !memcmp(mode, "a+", 2) ){
   return 1; 
  }
  return 0; 
}