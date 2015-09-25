#include "router.h"

typedef struct connectionObject{
  routerObject *router;
  char         *requestedFilename;
  char         *dataCache; 
  int          (*reinitialize)(struct connectionObject this); 
}connectionObject;


connectionObject *newConnection(void);