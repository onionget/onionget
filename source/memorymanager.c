#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "memorymanager.h"

static int memoryClear(volatile unsigned char *memoryPointer, uint64_t bytesize);

/*
 * secureAllocate returns NULL on error, otherwise returns a pointer to the allocated memory buffer, which is bytesize bytes and initialized to NULL. 
 */
void *secureAllocate(uint64_t bytesize)
{
  void *memory;
  
  if(bytesize == 0){
    printf("Error: Cannot allocate 0 bytes of memory\n");
    return NULL; 
  }
  
  memory = calloc(1, bytesize);
  
  if( memory == NULL ){
    printf("Error: Failed to allocate memory!\n");
    return NULL; 
  }
  
  return memory;
}

/*
 * returns 1 on success and 0 on error
 * 
 * memoryClear is passed a volatile unsigned char* pointing to bytesize bytes, clears each byte by setting to 0
 * 
 * implemented because the memset solution in MEM03-C causes compiler warnings, this should do the same thing without
 * compiler warning 
 */
static int memoryClear(volatile unsigned char *memoryPointer, uint64_t bytesize)
{
  if(memoryPointer == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }
  
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
 * implementation will be verified as not optimized out, however using a volatile pointer in compliance with MEM03-C, and also using a memory barried as 
 * suggested by various security experts. 
 * 
 * TODO: Make sure volatile pointer usage is in compliance with EXP32-C 
 * 
 */
int secureFree(void *memory, uint64_t bytesize)
{
  void                   **memoryCorrectCast; 
  volatile unsigned char *dataBuffer;

  //this function is actually passed a void**, declared void* in function definition for technical reasons
  memoryCorrectCast = (void**)memory; 
  
  //basic sanity checks
  if(*memoryCorrectCast == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  if(bytesize == 0){
    printf("Error: Zero bytes of memory is invalid\n");
    return 0;
  }

  //prepare to clear memory buffer
  dataBuffer = *(volatile unsigned char**)memoryCorrectCast; 
    
  //clear memory buffer, volatile pointer in compliance with MEM03-C
  if( !memoryClear(dataBuffer, bytesize) ){
    printf("Error: Failed to clear memory buffer\n");
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