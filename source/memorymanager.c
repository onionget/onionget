#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "memorymanager.h"

//returns NULL on error
void* secureAllocate(uint64_t bytesize)
{
  void *memory = calloc(1, bytesize);
  
  if( memory == NULL ){
    printf("Error: Failed to allocate memory!\n");
    return NULL; 
  }
  
  return memory;
}

//note: memory must be a void**
void secureFree(void* memory, uint64_t bytesize)
{
  volatile unsigned char *deoptimizedDataPointer;
  unsigned char          *dataBuffer;
  void                   **memoryCorrectCast;
  
  memoryCorrectCast = (void**)memory; 
  
  dataBuffer = *(unsigned char**)memoryCorrectCast; 
  
  deoptimizedDataPointer = dataBuffer;
  
  while(bytesize--) *deoptimizedDataPointer++ = 0;
  
  free(dataBuffer);
  
  *memoryCorrectCast = NULL; 
}