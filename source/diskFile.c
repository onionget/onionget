#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>

#include "dataContainer.h"
#include "diskFile.h"
#include "memoryManager.h"
#include "ogEnums.h"



//PUBLIC METHODS
static uint32_t              diskFileWrite(diskFileObject *this, dataContainerObject *dataContainer, uint32_t writeOffset); 
static dataContainerObject   *diskFileRead(diskFileObject *this, uint32_t bytesToRead, uint32_t readOffset);
static int                   closeTearDown(diskFileObject **thisPointer);
static long                  getBytesize(diskFileObject *this);


//PRIVATE METHODS
static int diskFileOpen(diskFileObject *this, char *mode);
static int fileModeReadable(char *mode);
static int fileModeWritable(char *mode);
static int fileModeValid(char *mode);
static int fileModeSeekable(char *mode); 
static int initializePathProperties(diskFileObject *this, char *path, char *name);

/************ OBJECT CONSTRUCTOR ******************/

/*
 * newDiskFile returns NULL on error and a new diskFile object on success. The file identified by /path/name is already open in the passed mode. 
 * path doesn't need to end with /, if it doesn't it will be added by initializePathProperties. 
 */
diskFileObject *newDiskFile(char *path, char *name, char *mode)
{
  diskFileObject  *this = NULL;
 
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
  this = (diskFileObject *)secureAllocate(sizeof(*this)); 
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
    secureFree(&this, sizeof(*this));
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
static long getBytesize(diskFileObject *this)
{
  long fileBytesize = 0; 
  
  if( this == NULL || this->descriptor == NULL ){
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
  
  if( fseek(this->descriptor, 0, SEEK_SET) == -1 ){
    printf("Error: Failed to seek to start of file\n");
    return -1; 
  }
  
  return fileBytesize; 
}



/*
 * closeTearDown returns 0 on error and 1 on success. It closes the file descriptor if it is open, and destroys the struct. (Not named destroy because it doesn't
 * destroy the file on the fisk and might be a confusing name). 
 */
static int closeTearDown(diskFileObject **thisPointer)
{
  diskFileObject *this = NULL;
  
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
  
  
  if( !secureFree(thisPointer, sizeof(**thisPointer)) ){
    printf("Error: Failed to tear down disk file\n");
    return 0;
  }
  
  return 1; 
}

/*
 * diskFileWrite returns 0 on error and bytes written on success
 */
static uint32_t diskFileWrite(diskFileObject *this, dataContainerObject *dataContainer, uint32_t writeOffset) 
{    
  int     fid; 
  void    *mappedMemory;
  uint32_t bytesToWrite;
  
  if(this == NULL || dataContainer == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  if(dataContainer->bytesize == 0){
    printf("Error: Cannot write 0 bytes to file\n");
    return 0; 
  }
  
  bytesToWrite = dataContainer->bytesize; 
  
  if( fileModeWritable(this->mode) != 1 ){
    printf("Error: File mode isn't writable (or it's NULL)\n"); 
    return 0; 
  }
  
  if(this->descriptor == NULL){
    printf("Error: File is not open");
    return 0;
  }
  
  fid = fileno(this->descriptor);
  if(fid == -1){
    printf("Error: Failed to get integer file descriptor\n");
    return 0; 
  }
  
  //TODO check that readOffset is divisible by sysconf(_SC_PAGE_SIZE) (currently hard coded as sanity check in controller) (this will be irrelevant if we support arbitrary page sizes)
  mappedMemory = mmap(NULL, bytesToWrite, PROT_WRITE, MAP_SHARED, fid , writeOffset);
  if( mappedMemory == MAP_FAILED){
    printf("Error: Failed to memory map file to write to\n");
    return 0;
  }
  
  memcpy(mappedMemory, dataContainer->data, bytesToWrite); 
  
  if( munmap(mappedMemory, bytesToWrite) ){
    printf("Error: Failed to unmap file memory\n");
    return 0;
  }
  
  return bytesToWrite;
}


/*
 * diskFileRead returns NULL on error and a pointer to a dataContainer holding the data from the file associated with the diskFile object on success.
 */
static dataContainerObject *diskFileRead(diskFileObject *this, uint32_t bytesToRead, uint32_t readOffset)
{
  dataContainerObject *dataContainer = NULL; 
  void                *mappedMemory  = NULL;
  int                 fid            = 0;
  
  if( this == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL; 
  }
  
  if( fileModeReadable(this->mode) != 1 ){
    printf("Error: File mode is not readable (or it's NULL)\n");
    return NULL; 
  }
    
  if(this->descriptor == NULL){
    printf("Error: File is not open\n");
    return NULL;
  }
    
  dataContainer = newDataContainer(bytesToRead);
  if(dataContainer == NULL){
    printf("Error: Failed to create data container to read file into\n");
    return NULL; 
  }

  fid = fileno(this->descriptor);
  if(fid == -1){
    printf("Error: Failed to get integer file descriptor\n");
    dataContainer->destroyDataContainer(&dataContainer); 
    return 0; 
  }

  //TODO check that readOffset is divisible by sysconf(_SC_PAGE_SIZE) (currently hard coded as sanity check in controller) (this will be irrelevant if we support arbitrary page sizes)
  mappedMemory = mmap(NULL, bytesToRead, PROT_READ, MAP_PRIVATE, fid , readOffset);
  if( mappedMemory == MAP_FAILED){
    printf("Error: Failed to memory map file to write to\n");
    dataContainer->destroyDataContainer(&dataContainer); 
    return 0;
  }
  
  memcpy(dataContainer->data, mappedMemory, bytesToRead);
  
  if( munmap(mappedMemory, bytesToRead) ){
    printf("Error: Failed to unmap memory read\n");
    dataContainer->destroyDataContainer(&dataContainer); 
    return 0; 
  }

  return dataContainer; 
}




/******** PRIVATE METHODS *********/

/*
 * initializePathProperties returns 0 on error and 1 on success. It initializes this->fullPath and this->fullPathBytesize. Note that this->fullPathBytesize
 * is one to two bytes larger than the exact memory used to hold the full path, because this->fullPath ends with one to two NULLs depending on if the passed
 * in path ends with a '/' or not. this->fullPathBytesize is exactly equal to the bytesize of this->fullPath though, counting the terminating NULL(s). 
 */
int initializePathProperties(diskFileObject *this, char *path, char *name)
{
  size_t  pathBytesize = 0;
  size_t  nameBytesize = 0;
  
  if(this == NULL || path == NULL || name == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }

  //get string sizes
  pathBytesize = strlen(path);
  nameBytesize = strlen(name);
  
  // +1 to ensure NULL termination, +1 to ensure room for terminating / if we need to add it. Wastes up to one byte of memory.
  this->fullPathBytesize = pathBytesize + nameBytesize + 2;  
  
  this->fullPath = (char *)secureAllocate(this->fullPathBytesize);
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
static int diskFileOpen(diskFileObject *this, char *mode)
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
  
  if(  !memcmp(mode, "w", 1) || !memcmp(mode, "a", 1) || !memcmp(mode, "r+", 2) || !memcmp(mode, "w+", 2) || !memcmp(mode, "a+", 2) || !memcmp(mode, "r", 1) ){
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
  
  if(  !memcmp(mode, "w", 1) || !memcmp(mode, "a", 1) || !memcmp(mode, "r+", 2) || !memcmp(mode, "w+", 2) || !memcmp(mode, "a+", 2) ){
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
  
  if( !memcmp(mode, "w", 1) || !memcmp(mode, "r+", 2) || !memcmp(mode, "w+", 2) || !memcmp(mode, "r", 1) ){
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
  
  if( !memcmp(mode, "r", 1) || !memcmp(mode, "r+", 2) || !memcmp(mode, "w+", 2) || !memcmp(mode, "a+", 2) ){
   return 1; 
  }
  return 0; 
}