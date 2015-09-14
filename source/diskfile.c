#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "datacontainer.h"
#include "diskfile.h"
#include "memorymanager.h"
#include "og_enums.h"



//PUBLIC METHODS
static int             diskFileWrite(diskFile *this, dataContainer *dataContainer);
static dataContainer   *diskFileRead(diskFile *this);
static int             closeTearDown(diskFile **thisPointer);
static long int        getBytesize(diskFile *this);


//PRIVATE METHODS
static int diskFileOpen(diskFile *this, char *mode);
static int fileModeReadable(char *mode);
static int fileModeWritable(char *mode);
static int fileModeValid(char *mode);
static int fileModeSeekable(char *mode); 
static int initializePathProperties(diskFile *this, char *path, char *name);

/************ OBJECT CONSTRUCTOR ******************/

/*
 * newDiskFile returns NULL on error and a new diskFile object on success. The file identified by /path/name is already open in the passed mode. 
 * path doesn't need to end with /, if it doesn't it will be added by initializePathProperties. 
 */
diskFile *newDiskFile(char *path, char *name, char *mode)
{
  diskFile  *this;
 
  if( path == NULL || name == NULL || mode == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL; 
  }
  
  //sanity check
  if(fileModeValid(mode) != 1){
    printf("Error: Invalid (or NULL) file mode specified\n");
    return NULL; 
  }
    

  //allocate memory for the object
  this = secureAllocate(sizeof(struct diskFile)); 
  if(this == NULL){
    printf("Error: Failed to allocate memory for disk file\n");
    return NULL; 
  }
  
  
  if( !initializePathProperties(this, path, name) ){
    printf("Error: Failed to initialize diskFile path\n");
    return NULL; 
  }
  
  
  //open the file in the specified mode
  if( !diskFileOpen(this, mode) ){
    printf("Error: Failed to open file\n");
    secureFree(&this->fullPath, this->fullPathBytesize);
    secureFree(&this, sizeof(struct diskFile));
    return NULL;
  }
  

  //initialize public methods
  this->diskFileWrite = &diskFileWrite;
  this->diskFileRead  = &diskFileRead; 
  this->closeTearDown = &closeTearDown;
  this->getBytesize   = &getBytesize; 
  
  
  return this; 
}



/******** PUBLIC METHODS *********/

/*
 * getBytesize returns -1 on error, and the bytesize of the initialized diskFile on success
 */
static long int getBytesize(diskFile *this)
{
  long int  fileBytesize; 
  
  if( this == NULL ){
    printf("Error: Something was NULL that shouldn't have been");
    return -1; 
  }
  
  if( fileModeSeekable(this->mode) != 1 ){
    printf("Error: File mode is not seekable (or it's NULL)");
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



/*
 * closeTearDown returns 0 on error and 1 on success. It closes the file descriptor if it is open, and destroys the struct. (Not named destroy because it doesn't
 * destroy the file on the fisk and might be a confusing name). 
 */
static int closeTearDown(diskFile **thisPointer)
{
  diskFile *this;
  
  this = *thisPointer; 
  
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  if(this->descriptor != NULL){
    if( fclose(this->descriptor) == EOF ){
      printf("Error: Failed to close file descriptor\n"); 
      return 0;
    }
  }
    
  if( !secureFree( &(this->fullPath), this->fullPathBytesize) ){
    printf("Error: Failed to free path\n");
    return 0;
  }
  
  
  if( !secureFree(thisPointer, sizeof(struct diskFile)) ){
    printf("Error: Failed to tear down disk file\n");
    return 0;
  }
  
  return 1; 
}

/*
 * diskFileWrite returns 0 on error and 1 on success. It writes the data in dataContainer to the file on the disk associated with the diskFile object. 
 */
static int diskFileWrite(diskFile *this, dataContainer *dataContainer) 
{  
  if(this == NULL || dataContainer == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  if( fileModeWritable(this->mode) != 1 ){
    printf("Error: File mode isn't writable (or it's NULL)\n"); 
    return 0; 
  }
  
  if(!this->descriptor){
    printf("Error: File is not open");
    return 0;
  }
  
  //TODO look more at error checking TODO also don't try to write the entire file at once!!
  if( fwrite( (const void*)dataContainer->data, dataContainer->bytesize, COUNT, this->descriptor) != COUNT ){
    printf("Error: didn't write all data to file\n");
    return 0; 
  }
  
  return 1; 
}


/*
 * diskFileRead returns NULL on error and a pointer to a dataContainer holding the data from the file associated with the diskFile object on success.
 */
static dataContainer *diskFileRead(diskFile *this)
{
  dataContainer *dataContainer; 
  long int      fileBytesize;

  if( this == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL; 
  }
  
  if( fileModeReadable(this->mode) != 1 ){
    printf("Error: File mode is not readable (or it's NULL)\n");
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

  
  //TODO look into error checking more TODO and maybe don't read in a single operation? 
  if( fread(dataContainer->data, fileBytesize, COUNT, this->descriptor) != COUNT){
    printf("Error: Failed to read file into data container\n");
    secureFree( &dataContainer->data, dataContainer->bytesize );
    secureFree( &dataContainer, sizeof(struct dataContainer)); 
    return NULL; 
  }
  

  return dataContainer; 
}




/******** PRIVATE METHODS *********/

/*
 * initializePathProperties returns 0 on error and 1 on success. It initializes this->fullPath and this->fullPathBytesize. Note that this->fullPathBytesize
 * is one to two bytes larger than the exact memory used to hold the full path, because this->fullPath ends with one to two NULLs depending on if the passed
 * in path ends with a '/' or not. this->fullPathBytesize is exactly equal to the bytesize of this->fullPath though, counting the terminating NULL(s). 
 */
int initializePathProperties(diskFile *this, char *path, char *name)
{
  uint32_t  pathBytesize;
  uint32_t  nameBytesize;
  
  if(this == NULL || path == NULL || name == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }

  //get string sizes
  pathBytesize = strlen(path);
  nameBytesize = strlen(name);
  
  // +1 to ensure NULL termination, +1 to ensure room for terminating / if we need to add it. Wastes up to one byte of memory.
  this->fullPathBytesize = pathBytesize + nameBytesize + 2;  
  
  this->fullPath = secureAllocate(this->fullPathBytesize);
  if(this->fullPath == NULL){
    printf("Error: Failed to allocate memory for path\n");
    return 0;
  }
     
  if(path[pathBytesize - 1] != '/'){
    strncat(this->fullPath, path, pathBytesize);
    strncat(this->fullPath, "/", 1);
    strncat(this->fullPath, name, nameBytesize); 
  }
  else{
    strncat(this->fullPath, path, pathBytesize);
    strncat(this->fullPath, name, nameBytesize); 
  }
  
  return 1; 
}


/*
 * diskFileOpen returns 0 on error and 1 on success. Sets this->descriptor to an fd for file at this->fullPath opened in mode mode, and sets this->mode to mode. 
 */
static int diskFileOpen(diskFile *this, char *mode)
{
  if( this == NULL || mode == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  this->descriptor = fopen( (const char*)this->fullPath, mode );
  if(this->descriptor == NULL){
    printf("Error: Failed to open file\n"); 
    return 0; 
  }
  
  this->mode = mode;
  
  return 1;
}


/*
 * fileModeValid returns -1 on error, 1 if the file mode is valid, and 0 if the file mode isn't valid. 
 */
static int fileModeValid(char *mode)
{
  if(mode == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return -1; 
  }
  
  if(  !memcmp(mode, "w", 1) | !memcmp(mode, "a", 1) | !memcmp(mode, "r+", 2) | !memcmp(mode, "w+", 2) | !memcmp(mode, "a+", 2) | !memcmp(mode, "r", 1) ){
    return 1; 
  } 
  return 0; 
}

/*
 * fileModeWritable returns -1 on error, 1 if the file mode is writable, and 0 if the file mode isn't writable. 
 */
static int fileModeWritable(char *mode)
{
  if(mode == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return -1; 
  }
  
  if(  !memcmp(mode, "w", 1) | !memcmp(mode, "a", 1) | !memcmp(mode, "r+", 2) | !memcmp(mode, "w+", 2) | !memcmp(mode, "a+", 2) ){
   return 1; 
  }
  return 0; 
}

/*
 * fileModeSeekable returns -1 on error, 1 if the file mode is seekable, and 0 if the file mode isn't seekable. 
 */
static int fileModeSeekable(char *mode)
{
  if(mode == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return -1; 
  }
  
  if( !memcmp(mode, "w", 1) | !memcmp(mode, "r+", 2) | !memcmp(mode, "w+", 2) | !memcmp(mode, "r", 1) ){
    return 1;
  }
  return 0;
}

/*
 * fileModeReadable returns -1 on error, 1 if the file mode is readable, and 0 if the file mode isn't readable. 
 */
static int fileModeReadable(char *mode)
{
  if(mode == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return -1; 
  }
  
  if( !memcmp(mode, "r", 1) | !memcmp(mode, "r+", 2) | !memcmp(mode, "w+", 2) | !memcmp(mode, "a+", 2) ){
   return 1; 
  }
  return 0; 
}