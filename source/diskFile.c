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


//private internal values 
typedef struct diskFilePrivate{
  diskFileObject publicDiskFile;
  char           *mode;
  char           *fullPath;
  uint32_t       fullPathBytesize; 
  FILE           *descriptor; 
}diskFilePrivate;


//PUBLIC METHODS
static uint32_t              dfWrite(diskFileObject *this, dataContainerObject *dataContainer, uint32_t writeOffset); 
static dataContainerObject   *dfRead(diskFileObject *this, uint32_t bytesToRead, uint32_t readOffset);
static int                   closeTearDown(diskFileObject **thisPointer);
static long                  dfBytesize(diskFileObject *this);
static int                   dfOpen(diskFileObject *this, char *path, char *name, char *mode);

//PRIVATE METHODS
static int fileModeReadable(char *mode);
static int fileModeWritable(char *mode);
static int fileModeValid(char *mode);
static int fileModeSeekable(char *mode); 
static int initializeFileProperties(diskFilePrivate *privateThis, char *path, char *name, char *mode);






/************ OBJECT CONSTRUCTOR ******************/


/*
 * newDiskFile returns NULL on error and a new diskFile object on success. 
 */
diskFileObject *newDiskFile(void)
{
  diskFilePrivate *privateThis = NULL;
  
      
  //allocate memory for the object
  privateThis = (diskFilePrivate *)secureAllocate(sizeof(*privateThis)); 
  if(privateThis == NULL){
    printf("Error: Failed to allocate memory for disk file\n");
    return NULL; 
  }
  

  //initialize public methods
  privateThis->publicDiskFile.dfOpen          = &dfOpen;
  privateThis->publicDiskFile.dfWrite         = &dfWrite;
  privateThis->publicDiskFile.dfRead          = &dfRead; 
  privateThis->publicDiskFile.closeTearDown   = &closeTearDown;
  privateThis->publicDiskFile.dfBytesize      = &dfBytesize; 
  

  //initialize private properties 
  privateThis->mode             = NULL;
  privateThis->fullPath         = NULL;
  privateThis->fullPathBytesize = 0;
  privateThis->descriptor       = NULL;
  

  return (diskFileObject *) privateThis; 
}



/******** PUBLIC METHODS *********/

/*
 * dfBytesize returns -1 on error, and the bytesize of the initialized diskFile on success
 */
static long dfBytesize(diskFileObject *this)
{
  long            fileBytesize = 0;
  diskFilePrivate *privateThis = NULL;
  
  privateThis = (diskFilePrivate *) this; 
  
  if( privateThis == NULL || privateThis->descriptor == NULL ){
    printf("Error: Something was NULL that shouldn't have been");
    return -1; 
  }
  
  if( fileModeSeekable(privateThis->mode) != 1 ){
    printf("Error: File mode is not seekable (or it's NULL)");
    return -1; 
  }
    
  if( fseek(privateThis->descriptor, 0, SEEK_END) == -1 ){
    printf("Error: Failed to seek to end of file\n");
    return -1; 
  }
  
  if( (fileBytesize = ftell(privateThis->descriptor)) == -1 ){
    printf("Error: Failed to get file bytesize\n");
    return -1; 
  }
  
  if( fseek(privateThis->descriptor, 0, SEEK_SET) == -1 ){
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
  diskFilePrivate **privateThisPointer = NULL;
  diskFilePrivate *privateThis         = NULL; 
  
  privateThisPointer = (diskFilePrivate**)thisPointer; 
  privateThis        = *privateThisPointer; 
  
  if(privateThis == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  if(privateThis->descriptor != NULL){
    if( fclose(privateThis->descriptor) == EOF ){
      printf("Error: Failed to close file descriptor\n"); 
      return 0;
    }
  }
    
  if( !secureFree( &(privateThis->fullPath), privateThis->fullPathBytesize) ){
    printf("Error: Failed to free path\n");
    return 0;
  }
  
  
  if( !secureFree(privateThisPointer, sizeof(diskFilePrivate)) ){
    printf("Error: Failed to tear down disk file\n");
    return 0;
  }
  
  return 1; 
}

/*
 * dfWrite returns 0 on error and bytes written on success
 */
static uint32_t dfWrite(diskFileObject *this, dataContainerObject *dataContainer, uint32_t writeOffset) 
{    
  int             fid            = 0; 
  void            *mappedMemory  = NULL;
  uint32_t        bytesToWrite   = 0;
  diskFilePrivate *privateThis   = NULL;
  
  privateThis = (diskFilePrivate *)this; 
  
  if(privateThis == NULL || dataContainer == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  if(dataContainer->bytesize == 0){
    printf("Error: Cannot write 0 bytes to file\n");
    return 0; 
  }
  
  bytesToWrite = dataContainer->bytesize; 
  
  if( fileModeWritable(privateThis->mode) != 1 ){
    printf("Error: File mode isn't writable (or it's NULL)\n"); 
    return 0; 
  }
  
  if(privateThis->descriptor == NULL){
    printf("Error: File is not open");
    return 0;
  }
  
  fid = fileno(privateThis->descriptor);
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
 * dfRead returns NULL on error and a pointer to a dataContainer holding the data from the file associated with the diskFile object on success.
 */
static dataContainerObject *dfRead(diskFileObject *this, uint32_t bytesToRead, uint32_t readOffset)
{
  dataContainerObject *dataContainer = NULL; 
  void                *mappedMemory  = NULL;
  int                 fid            = 0;
  diskFilePrivate     *privateThis   = NULL;
  
  privateThis = (diskFilePrivate *)this; 
  
  if( privateThis == NULL ){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL; 
  }
  
  if( fileModeReadable(privateThis->mode) != 1 ){
    printf("Error: File mode is not readable (or it's NULL)\n");
    return NULL; 
  }
    
  if(privateThis->descriptor == NULL){
    printf("Error: File is not open\n");
    return NULL;
  }
    
  dataContainer = newDataContainer(bytesToRead);
  if(dataContainer == NULL){
    printf("Error: Failed to create data container to read file into\n");
    return NULL; 
  }

  fid = fileno(privateThis->descriptor);
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


/*
 * dfOpen returns 0 on error and 1 on success. Sets this->descriptor to an fd for file at this->fullPath opened in mode mode, and sets this->mode to mode. 
 */
static int dfOpen(diskFileObject *this, char *path, char *name, char *mode)
{
  diskFilePrivate *privateThis = NULL; 
  
  privateThis = (diskFilePrivate *)this; 
  
  if( privateThis == NULL || path == NULL || name == NULL || mode == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  if( !initializeFileProperties(privateThis, path, name, mode) ){
    printf("Error: Failed to initialize diskFile properties\n");
    return 0; 
  }
  
  privateThis->descriptor = fopen( (const char*)privateThis->fullPath, privateThis->mode );
  if(privateThis->descriptor == NULL){
    printf("Error: Failed to open file\n"); //TODO reset object state? 
    return 0; 
  }
 
  return 1;
}



/******** PRIVATE METHODS *********/

/*
 * initializeFileProperties returns 0 on error and 1 on success.  
 */
int initializeFileProperties(diskFilePrivate *privateThis, char *path, char *name, char *mode)
{
  size_t  pathBytesize = 0;
  size_t  nameBytesize = 0;
  
  if(privateThis == NULL || path == NULL || name == NULL || mode == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }

  if(fileModeValid(mode) != 1){
    printf("Error: Invalid (or NULL) file mode specified\n");
    return 0; 
  }
  
  //get string sizes
  pathBytesize = strlen(path);
  nameBytesize = strlen(name);
  
  // +1 to ensure NULL termination, +1 to ensure room for terminating / if we need to add it. Wastes up to one byte of memory.
  privateThis->fullPathBytesize = pathBytesize + nameBytesize + 2;  
  
  privateThis->fullPath = (char *)secureAllocate(privateThis->fullPathBytesize);
  if(privateThis->fullPath == NULL){
    printf("Error: Failed to allocate memory for path\n");
    return 0;
  }
     
  if(path[pathBytesize - 1] != '/'){
    strncat(privateThis->fullPath, path, pathBytesize);
    strncat(privateThis->fullPath, "/", 1);
    strncat(privateThis->fullPath, name, nameBytesize); 
  }
  else{
    strncat(privateThis->fullPath, path, pathBytesize);
    strncat(privateThis->fullPath, name, nameBytesize); 
  }
  
  privateThis->mode = mode; 
  
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