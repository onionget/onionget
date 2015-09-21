#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "fileBank.h"
#include "dataContainer.h"
#include "memoryManager.h"



/*
//initialize new fileBankObject
fileBankObject *newFileBank(void)
{
  fileBankObject *fileBank; 
  dllObject      *fileObjects;
  
  //allocate memory for the fileBankObject
  fileBank = (fileBankObject *)secureAllocate(sizeof(*fileBank)); 
  //allocate memory for the fileBankObjects private properties
  fileBank->fileBankPrivate = (fileBankPrivate)secureAllocate(sizeof(struct fileBankPrivate)); 

  
  //initialize private properties, the DLL for files
  fileBankObject->fileBankPrivate->fileObjects = newDll(); 
  
  
  fileBank->getFile = &getFile; 
}
*/


/*
 *  getId returns NULL if an error occurs or the item identified by id is not found in the list. On success it returns a pointer to to requested dataContainer.
 */
/*
static dataContainerObject *getFile(fileBankObject this, char *id, uint32_t idBytesize)
{
  dllMember *currentMember;
  
  if(this == NULL || id == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return NULL; 
  }
  
  for(currentMember = this->head; currentMember != NULL; currentMember = currentMember->next){
    if( !memcmp(currentMember->identifier, id, idBytesize) ){
      return currentMember->dataContainer;
    }
  }
   
  return NULL; 
}
*/ 