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
#include "macros.h"


//private internal values 
typedef struct diskFilePrivate{
  diskFileObject publicDiskFile;
  char           *mode;
  char           *fullPath;
  uint32_t       fullPathBytesize; 
  uint32_t       bytesize; 
  FILE           *descriptor; 
}diskFilePrivate;


//PUBLIC METHODS
static uint32_t              dfWrite(diskFileObject *this, void *dataBuffer, size_t bytesize, uint32_t writeOffset); 
static dataContainerObject   *dfRead(diskFileObject *this, uint32_t bytesToRead, uint32_t readOffset);
static int                   closeTearDown(diskFileObject **thisPointer);
static uint32_t              dfBytesize(diskFileObject *this);
static int                   dfOpen(diskFileObject *this, char *path, char *name, char *mode);

//PRIVATE METHODS
static int fileModeReadable(char *mode);
static int fileModeWritable(char *mode);
static int fileModeValid(char *mode);
static int fileModeSeekable(char *mode); 
static int initializeFileProperties(diskFileObject *this, char *path, char *name, char *mode);






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
    logEvent("Error", "Failed to allocate memory for disk file");
    return NULL; 
  }
  

  //initialize public methods
  privateThis->publicDiskFile.dfOpen          = &dfOpen;
  privateThis->publicDiskFile.dfWrite         = &dfWrite;
  privateThis->publicDiskFile.dfRead          = &dfRead; 
  privateThis->publicDiskFile.closeTearDown   = &closeTearDown;
  privateThis->publicDiskFile.getBytesize     = &getBytesize;
  

  //initialize private properties 
  privateThis->mode             = NULL;
  privateThis->fullPath         = NULL;
  privateThis->fullPathBytesize = 0;
  privateThis->descriptor       = NULL;
  privateThis->bytesize         = -1; 
  

  return (diskFileObject *) privateThis; 
}



/******** PUBLIC METHODS *********/





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
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  if(privateThis->descriptor != NULL){
    if( fclose(privateThis->descriptor) == EOF ){
      logEvent("Error", "Failed to close file descriptor"); 
      return 0;
    }
  }
    
  if( !secureFree( &(privateThis->fullPath), privateThis->fullPathBytesize) ){
    logEvent("Error", "Failed to free path");
    return 0;
  }
  
  
  if( !secureFree(privateThisPointer, sizeof(diskFilePrivate)) ){
    logEvent("Error", "Failed to tear down disk file");
    return 0;
  }
  
  return 1; 
}

/*
 * dfWrite returns 0 on error and bytes written on success
 */
static uint32_t dfWrite(diskFileObject *this, void *dataBuffer, size_t bytesize, uint32_t writeOffset) 
{    
  int             fid            = 0; 
  void            *mappedMemory  = NULL;
  diskFilePrivate *private       = NULL;
  
  private = (diskFilePrivate *)this; 
  
  if(private == NULL || this == NULL || dataBuffer == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  if(bytesize == 0){
    logEvent("Error", "Cannot write 0 bytes to file");
    return 0; 
  }
    
  if( fileModeWritable(private->mode) != 1 ){
    logEvent("Error", "File mode isn't writable (or it's NULL)"); 
    return 0; 
  }
  
  if(private->descriptor == NULL){
    logEvent("Error", "File is not open");
    return 0;
  }
  
  fid = fileno(private->descriptor);
  if(fid == -1){
    logEvent("Error", "Failed to get integer file descriptor");
    return 0; 
  }
  
  //TODO check that readOffset is divisible by sysconf(_SC_PAGE_SIZE) (currently hard coded as sanity check in controller) (this will be irrelevant if we support arbitrary page sizes)
  mappedMemory = mmap(NULL, bytesize, PROT_WRITE, MAP_SHARED, fid , writeOffset);
  if( mappedMemory == MAP_FAILED){
    logEvent("Error", "Failed to memory map file to write to");
    return 0;
  }
  
  memcpy(mappedMemory, dataBuffer, bytesize); 
  
  if( munmap(mappedMemory, bytesize) ){
    logEvent("Error", "Failed to unmap file memory");
    return 0;
  }
  
  return bytesize;
}


/*
 * dfRead returns 0 on error and 1 on success WARNING make sure that mmap with bytesToRead larger than file bytes that exist is secure, or that it will never happening TODO
 */
static int *dfRead(diskFileObject *this, void** outBuffer, uint32_t bytesToRead, uint32_t readOffset)
{
  int                 fid            = 0;
  diskFilePrivate     *private       = NULL;
  void                *mmapAddr      = NULL; 
  
  private = (diskFilePrivate *)this; 
  
  if( private == NULL || this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0; 
  }
  
  if( fileModeReadable(private->mode) != 1 ){
    logEvent("Error", "File mode is not readable (or it's NULL)");
    return 0; 
  }
    
  if(private->descriptor == NULL){
    logEvent("Error", "File is not open");
    return 0;
  }
    

  fid = fileno(private->descriptor);
  if(fid == -1){
    logEvent("Error", "Failed to get integer file descriptor");
    return 0; 
  }

  //TODO check that readOffset is divisible by sysconf(_SC_PAGE_SIZE) (currently hard coded as sanity check in controller) (this will be irrelevant if we support arbitrary page sizes)
  mmapAddr = mmap(*outBuffer, bytesToRead, PROT_READ, MAP_PRIVATE | MAP_LOCKED, fid , readOffset); //NOTE need to mlock stillread mmap man pages
  if( mappedMemory == MAP_FAILED){
    logEvent("Error", "Failed to memory map file to write to");
    return 0;
  }
  
  *outBuffer = mmapAddr; 
  

  


  return 1; 
}


/*
 * dfOpen returns 0 on error and 1 on success. Sets this->descriptor to an fd for file at this->fullPath opened in mode mode, and sets this->mode to mode. 
 */
static int dfOpen(diskFileObject *this, char *path, char *name, char *mode)
{
  diskFilePrivate *private = NULL; 
  
  private = (diskFilePrivate *)this; 
  
  if( private == NULL || this == NULL || path == NULL || name == NULL || mode == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  if( !initializeFileProperties(this, path, name, mode) ){
    logEvent("Error", "Failed to initialize diskFile properties");
    return 0; 
  }
  
  private->descriptor = fopen( (const char*)private->fullPath, private->mode );
  if(private->descriptor == NULL){
    logEvent("Error", "Failed to open file"); //TODO reset object state? 
    return 0; 
  }
  
  if( fileModeSeekable(private->mode) == 1 ){ 
    private->bytesize = this->dfBytesize(this); 
    if(private->bytesize == -1){
      logEvent("Error", "Failed to determine file bytesize");
      return 0; 
    }
  }
 
  return 1;
}


static uint32_t getBytesize(diskFile *this)
{
  diskFileprivate *private = (diskFilePrivate *)this;
  
  if(private == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return -1;
  }
  
  return private->bytesize; 
}


/******** PRIVATE METHODS *********/


/*
 * dfBytesize returns -1 on error, and the bytesize of the initialized diskFile on success
 */
static uint32_t dfBytesize(diskFileObject *this)
{
  long            fileBytesize = 0;
  diskFilePrivate *private     = NULL;
  
  private = (diskFilePrivate *) this; 
  
  if( private == NULL || this == NULL || private->descriptor == NULL ){ //this is always NULL if private it, but for readability
    logEvent("Error", "Something was NULL that shouldn't have been");
    return -1; 
  }
  
  if( fileModeSeekable(private->mode) != 1 ){
    logEvent("Error", "File mode is not seekable (or it's NULL)");
    return -1; 
  }
    
  if( fseek(private->descriptor, 0, SEEK_END) == -1 ){
    logEvent("Error", "Failed to seek to end of file");
    return -1; 
  }
  
  if( (fileBytesize = ftell(private->descriptor)) == -1 ){
    logEvent("Error", "Failed to get file bytesize");
    return -1; 
  }
  
  if( fseek(private->descriptor, 0, SEEK_SET) == -1 ){
    logEvent("Error", "Failed to seek to start of file");
    return -1; 
  }
  
  return (uint32_t)fileBytesize; //TODO need to go over all interger usage and make sure it is not screwing up anywhere, for now just hoping this is working correctly WARNING (WARNING long is more bytes than uint32_t so truncation is certain for large values this is a security problem but leaving for now) TODO TODO TODO
}



/*
 * initializeFileProperties returns 0 on error and 1 on success.  
 */
int initializeFileProperties(diskFileObject *this, char *path, char *name, char *mode)
{
  diskFilePrivate  *private     = NULL;
  size_t           pathBytesize = 0;
  size_t           nameBytesize = 0;
  
  private = (diskFilePrivate *)this; 
  
  if(private == NULL || this == NULL || path == NULL || name == NULL || mode == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }

  if(fileModeValid(mode) != 1){
    logEvent("Error", "Invalid (or NULL) file mode specified");
    return 0; 
  }
  
  //get string sizes
  pathBytesize = strlen(path);
  nameBytesize = strlen(name);
  
  // +1 to ensure NULL termination, +1 to ensure room for terminating / if we need to add it. Wastes up to one byte of memory.
  private->fullPathBytesize = pathBytesize + nameBytesize + 2;  
  
  private->fullPath = (char *)secureAllocate(private->fullPathBytesize);
  if(private->fullPath == NULL){
    logEvent("Error", "Failed to allocate memory for path");
    return 0;
  }
     
  if(path[pathBytesize - 1] != '/'){
    strncat(private->fullPath, path, pathBytesize);
    strncat(private->fullPath, "/", 1);
    strncat(private->fullPath, name, nameBytesize); 
  }
  else{
    strncat(private->fullPath, path, pathBytesize);
    strncat(private->fullPath, name, nameBytesize); 
  }
  
  private->mode = mode; 
  
  return 1; 
}


/*
 * fileModeValid returns -1 on error, 1 if the file mode is valid, and 0 if the file mode isn't valid. 
 */
static int fileModeValid(char *mode)
{
  if(mode == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
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
    logEvent("Error", "Something was NULL that shouldn't have been");
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
    logEvent("Error", "Something was NULL that shouldn't have been");
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
    logEvent("Error", "Something was NULL that shouldn't have been");
    return -1; 
  }
  
  if( !memcmp(mode, "r", 1) || !memcmp(mode, "r+", 2) || !memcmp(mode, "w+", 2) || !memcmp(mode, "a+", 2) ){
   return 1; 
  }
  return 0; 
}