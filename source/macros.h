#define logEvent(category, message) (ogLogFunction((category), (message), (__FILE__) , (__LINE__) ))


//never call this directly but always with macro logEvent(category, message) 
void ogLogMacroBackEnd(char *category, char *message, char *filename, unsigned int lineNumber); 