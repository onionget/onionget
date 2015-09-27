#pragma once
#include <stdint.h>

//note want to support uint64_t file transfers so need to implement htonll TODO

typedef struct routerObject{
  int (*socks5Connect)(struct routerObject *this, char *destAddress, uint8_t destAddressBytesize, uint16_t destPort);
  int (*receive)(struct routerObject *this, void *receiveBuffer, uint32_t payloadBytesize);
  int (*transmit)(struct routerObject *this, void *payload, uint32_t payloadBytesize);
  int (*transmitBytesize)(struct routerObject *this, uint32_t bytesize);
  uint32_t (*getIncomingBytesize)(struct routerObject *this);
  int (*ipv4Connect)(struct routerObject *this, char *ipv4Address, char *port);
  int (*ipv4Listen)(struct routerObject *this, char *address, int port);
  int  (*getConnection)(struct routerObject *this);
  int  (*setSocket)(struct routerObject *this, int socket);
  int (*destroyRouter)(struct routerObject **thisPointer); 
  int (*reinitialize)(struct routerObject *this); 
}routerObject;


routerObject* newRouter(void);

