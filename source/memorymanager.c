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
  
  memory = calloc(1, bytesize);
  
  if( memory == NULL ){
    printf("Error: Failed to allocate memory!\n");
    return NULL; 
  }
  
  return memory;
}

/*
 * When passed a void** that dereferences to a void* pointing to bytesize bytes, secureFree first clears the buffer pointed to by setting it to all nuills
 * and then it points the pointer to NULL. Returns 1 on success and 0 on error.  NOTE that memory must be a void** despite being a void* in the definition. 
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
  
  memoryCorrectCast = (void**)memory; 
  
  dataBuffer = *(unsigned char**)memoryCorrectCast; 
  
  deoptimizedDataPointer = dataBuffer;
  
  while(bytesize--) *deoptimizedDataPointer++ = 0;
  
  free(dataBuffer);
  
  *memoryCorrectCast = NULL; 
  
  return 1; 
}