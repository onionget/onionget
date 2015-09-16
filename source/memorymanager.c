#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "memorymanager.h"

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
 * When passed a void** that dereferences to a void* pointing to bytesize bytes, secureFree first clears the buffer pointed to by setting it to all nuills
 * and then it points the pointer to NULL. Returns 1 on success and 0 on error.  NOTE that memory must be a void** despite being a void* in the definition. TODO make sure pointer arithmetic is correct 
 * TODO make sure memory barrier is correctly implemented, and generally just look really close at this function, preferably replace it with one of the standards
 * for memory zero TODO make sure volatile use conforms with EXP32-C
 */
int secureFree(void *memory, uint64_t bytesize)
{
  volatile unsigned char *deoptimizedDataPointer;
  unsigned char          *dataBuffer;
  void                   **memoryCorrectCast;
  
  if(memory == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  if(bytesize == 0){
    printf("Error: Zero bytes of memory is invalid\n");
    return 0;
  }

  
  memoryCorrectCast = (void**)memory; 
  
  dataBuffer = *(unsigned char**)memoryCorrectCast; 
  
  deoptimizedDataPointer = dataBuffer;
  
  while(bytesize--) *deoptimizedDataPointer++ = 0;
  
  __asm__ __volatile__ ( "" : : "r"(dataBuffer) : "memory" );
  
  free(dataBuffer);
  
  *memoryCorrectCast = NULL; 
  
  return 1; 
}