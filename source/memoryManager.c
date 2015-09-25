#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "memoryManager.h"
#include "macros.h"



/*
 * secureAllocate returns NULL on error, otherwise returns a pointer to the allocated memory buffer, which is bytesize bytes and initialized to NULL. 
 */
void *secureAllocate(size_t bytesize)
{
  void *memory;
  
  if(bytesize == 0){
    logEvent("Error", "Cannot allocate 0 bytes of memory");
    return NULL; 
  }
  
  memory = calloc(1, bytesize);
  
  if( memory == NULL ){
    logEvent("Error", "Failed to allocate memory!");
    return NULL; 
  }
  
  return memory;
}

/*
 * returns 1 on success and 0 on error
 * 
 * memoryClear is passed a void* pointing to bytesize bytes, clears each byte by setting to 0
 * 
 * implemented because the memset solution in MEM03-C causes compiler warnings, this should do the same thing without
 * compiler warning 
 */
int memoryClear(void *memoryPointerV, size_t bytesize)
{
  volatile unsigned char *memoryPointer = NULL;
  
  if(memoryPointerV == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0; 
  }
  
  memoryPointer = memoryPointerV; 
  
  while(bytesize--){
    *memoryPointer++ = 0;
  }
  
  return 1; 
}


/* NOTE: memoryPointerPointer must be a void** despite being a void* in the function definition 
 * 
 * Returns 1 on success and 0 on error 
 * 
 * Passed a void* that casts to a void** that points to a pointer pointing to bytesize bytes, sets each byte to NULL in compliance with MEM03-C, 
 * frees the memory, and points the pointer to NULL
 * 
 * Superficial testing confirms memory is freed, pointer is set to NULL, and memory is cleared, though more in depth testing of memory cleared is required
 * TODO: preferably the memory clearing function will be replaced with explicit_bzero, memset_s, memzero_explicit, or similar, or at least the custom 
 * implementation will be verified as not optimized out, however using a volatile pointer in compliance with MEM03-C, and also using a memory barrier as 
 * suggested by various security experts. 
 * 
 */
int secureFree(void *memory, size_t bytesize)
{
  void **memoryCorrectCast; 
  void *dataBuffer;

  //this function is actually passed a void**, declared void* in function definition for technical reasons
  memoryCorrectCast = (void**)memory; 
  
  //basic sanity checks
  if(*memoryCorrectCast == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  if(bytesize == 0){
    logEvent("Error", "Zero bytes of memory is invalid");
    return 0;
  }

  //prepare to clear memory buffer
  dataBuffer = *(void**)memoryCorrectCast; 
    
  //clear memory buffer, volatile pointer in compliance with MEM03-C
  if( !memoryClear(dataBuffer, bytesize) ){
    logEvent("Error", "Failed to clear memory buffer");
    return 0;
  }

  //attempt to ensure that memory clear is not optimized out
  //memory barrier in compliance with https://sourceware.org/ml/libc-alpha/2014-12/msg00506.html
  __asm__ __volatile__ ( "" : : "r"(dataBuffer) : "memory" );
  
  //tested with valgrind as correctly freeing memory
  free(*memoryCorrectCast);
  
  //tested to confirm proper pointer set to NULL in compliance with MEM01-C
  *memoryCorrectCast = NULL; 
  
  return 1; 
}