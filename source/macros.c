#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *getTimeString(void);


/*** For logging macro ***/ 
//TODO time function was acting very weird when it was in the ogLogMacroBackEnd function, this warrants looking into

//never call this directly but always with macro logEvent(category, message) 
void ogLogMacroBackEnd(char *category, char *message, char *filename, unsigned int lineNumber) 
{
  char *timeString = getTimeString(); 
  
  
  if(category == NULL || message == NULL || filename == NULL || timeString == NULL){
    printf("Error: Failed to log error, but didn't fail to log log error error\n");
    return; 
  }
  
  printf("%s: %s --- %s:%u --- %s \n", category, message, filename, lineNumber, timeString); 
}


//returns string from ctime function or NULL on error
static char *getTimeString()
{
  time_t timeInSeconds;
  char   *timeInString;
  
  timeInSeconds = time(NULL);
  if(timeInSeconds == (time_t) (-1)){
    return NULL; 
  }
  
  timeInString = ctime((const time_t *)&timeInSeconds); 
  if(timeInString == NULL){
    return NULL; 
  }
  
  return timeInString; 
}