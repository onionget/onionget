#define logEvent(category, message) (ogLogMacroBackEnd((category), (message), (__FILE__) , (__LINE__) ))


//never call this directly but always with macro logEvent(category, message) 
void ogLogMacroBackEnd(char *category, char *message, char *filename, unsigned int lineNumber); 