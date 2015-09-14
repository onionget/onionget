#pragma once
#include <stdint.h>
#include "datacontainer.h"


typedef struct router{
  int socket;
  
  int (*socks5Connect)(struct router *this, char *destAddress, uint8_t destAddressBytesize, uint16_t destPort);
  dataContainer *(*receive)(struct router *this, uint32_t payloadBytesize);
  int (*transmit)(struct router *this, void *payload, uint32_t payloadBytesize);
  int (*transmitBytesize)(struct router *this, uint32_t bytesize);
  uint32_t (*getIncomingBytesize)(struct router *this);
  int (*ipv4Connect)(struct router *this, char *address, char *port);
  int (*ipv4Listen)(struct router *this, char *address, int port);
  int  (*getConnection)(struct router *this);
  int  (*setSocket)(struct router *this, int socket);
  int (*destroyRouter)(struct router **thisPointer); 
}router;


router* newRouter(void);

