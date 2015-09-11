#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <sys/time.h> 
#include <stdint.h>



//
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
//

#include "router.h"
#include "datacontainer.h"
#include "memorymanager.h"

enum{ RECEIVE_WAIT_TIMEOUT_SECONDS = 30 };
enum{ RECEIVE_WAIT_TIMEOUT_USECS   = 0  };


//public methods
static dataContainer* receive(router* this, uint32_t payloadBytesize);
static int transmit(router* this, void* payload, uint32_t payloadBytesize);
static int socks5Connect(router* this, char* destAddress, uint8_t destAddressBytesize, uint16_t destPort);
static int transmitBytesize(router *this, uint32_t bytesize);
static uint32_t getIncomingByteize(router *this); //note that this only gets incoming bytesize if the incoming bytesize is actually sent, as a uint32_t 
static int ipv4Connect(router* this, char* address, char* port);
static void setSocket(router* this, int socket);


static int ipv4Listen(router* this, char* address, int port);
static int getConnection(router* this);

static int destroyRouter(router** thisPointer);




/********** CONSTRUCT ****************/
router* newRouter()
{
  //allocate memory for the object
  router *this   = secureAllocate(sizeof(router));
  
  //object properties
  this->socket   = -1; 
 
  //object public methods
  this->transmit              = &transmit;
  this->transmitBytesize      = &transmitBytesize;
  this->receive               = &receive;
  this->socks5Connect         = &socks5Connect;
  this->getIncomingBytesize   = &getIncomingByteize;
  this->ipv4Connect           = &ipv4Connect; 
  this->ipv4Listen            = &ipv4Listen;
  this->getConnection         = &getConnection;
  this->setSocket             = &setSocket; 
  this->destroyRouter         = &destroyRouter;
  
 
  return this; 
}



static int destroyRouter(router** thisPointer)
{
  router *this = *thisPointer;
  
  if( close(this->socket) != 0 ){
    printf("Error: Failed to close socket\n");
    return 0; 
  }
  
  secureFree(thisPointer, sizeof(router)); 
  return 1; 
}


/********** PUBLIC METHODS ****************/

//returns NULL on error
static dataContainer* receive(router* this, uint32_t payloadBytesize)
{
  dataContainer   *receivedMessage;
  uint32_t        bytesReceived;
  uint32_t        recvReturn;
  
  receivedMessage = newDataContainer(payloadBytesize);
  if(receivedMessage == NULL){
    printf("Error: Failed to allocate data container for received messager\n");
    return NULL;
  }
    
  for(bytesReceived = 0, recvReturn = 0; bytesReceived != payloadBytesize; bytesReceived += recvReturn){
    
    recvReturn = recv(this->socket, &(receivedMessage->data[bytesReceived]), payloadBytesize - bytesReceived, 0);     
    if(recvReturn == -1 || recvReturn == 0){ 
      printf("Error: Failed to receive bytes\n");
      return NULL; 
    }     
  }
    
  return receivedMessage;
}

//returns -1 on error
static uint32_t getIncomingByteize(router *this)
{
  uint32_t      incomingBytesize;
  dataContainer *incomingBytesizeContainer;
  
  incomingBytesizeContainer = this->receive(this, sizeof(uint32_t));
  if(incomingBytesizeContainer == NULL){
    printf("Error: Failed to get incoming bytesize\n");
    return -1;  
  }
  
  incomingBytesize = *((uint32_t*)incomingBytesizeContainer->data);
  
  incomingBytesizeContainer->destroyDataContainer(&incomingBytesizeContainer);
  
  
  return ntohl(incomingBytesize);
}

//returns 0 on error
static int transmit(router* this, void* payload, uint32_t payloadBytesize)
{
  uint32_t sentBytes;  
  uint32_t sendReturn;
  
  for(sentBytes = 0, sendReturn = 0; sentBytes != payloadBytesize; sentBytes += sendReturn){
    sendReturn = send(this->socket, payload, payloadBytesize - sentBytes, 0); //TODO not current keeping track of buffer position
    if(sendReturn == -1){
      printf("Error: Failed to send bytes\n");
      return 0;
    } 
  }
 
 return 1; 
}

//returns 0 on error
static int transmitBytesize(router *this, uint32_t bytesize)
{
  uint32_t bytesizeEncoded = htonl(bytesize);
  
  if( !this->transmit(this, &bytesizeEncoded, sizeof(uint32_t)) ){
    printf("Error: Failed to transmit bytesize\n");
    return 0; 
  }
  
  return 1; 
}


//returns 0 on error
//comments reference https://www.ietf.org/rfc/rfc1928.txt
static int socks5Connect(router* this, char* destAddress, uint8_t destAddressBytesize, uint16_t destPort)
{
  dataContainer *socksRequest;
  dataContainer *proxyResponse;
  int           requestBytesize;

  //desired destination port in network octet order
  destPort = htons(destPort);
  
  // | VER | NMETHODS | METHODS |
  //socks version 5, one method, no authentication
  if( !this->transmit(this, "\005\001\000", 3) ){
    printf("Error: Socks connection failed to transmit bytes to socks server\n");
    return 0;
  }
  
  // | VER | METHOD |
  proxyResponse = this->receive(this, 2);
  if(proxyResponse == NULL){
    printf("Error: Socks connection failed to receive response from socks server\n");
    return 0;
  }
    
  if(proxyResponse->data[0] != 5){
    printf("Error: Proxy doesn't think it is socks 5");
    return 0; 
  }
  
  if(proxyResponse->data[1] != 0){
    printf("Error: Proxy expects authentication");
    return 0; 
  }
  
  proxyResponse->destroyDataContainer(&proxyResponse); 
  
 /* CLIENT REQUEST FORMAT
  * 
  * note destAddress is prepended with one octet specifying its bytesize
  * 
  * +----+-----+-------+------+----------+----------+
  * |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
  * +----+-----+-------+------+----------+----------+
  * | 1  |  1  | X'00' |  1   | Variable |    2     |
  * +----+-----+-------+------+----------+----------+
  *
  */
  requestBytesize = 1 + 1 + 1 + 1 + destAddressBytesize + 1 + 2;
  

  
  socksRequest = newDataContainer(requestBytesize);
  if(socksRequest == NULL){
    printf("Error: Failed to create data container for socks request\n");
    return 0;
  }
    
  //first four bytes: socks version 5, command is connect, rsv is null, identifier is domain name 
  memcpy(socksRequest->data, "\005\001\000\003", 4);
  //fifth byte: prepend identifier with its bytesize 
  memcpy(&socksRequest->data[4], &destAddressBytesize, 1);
  //variable bytes: the destination identifier
  memcpy(&socksRequest->data[5], destAddress, destAddressBytesize);
  //final two bytes: the destination port in network octet order
  memcpy(&socksRequest->data[5 + destAddressBytesize], &destPort, 2);
  
  
  if( !this->transmit(this, socksRequest->data, requestBytesize) ){
    printf("Error: Failed to transmit message to socks server\n");
    return 0;
  }
  
  socksRequest->destroyDataContainer(&socksRequest); 
  
  
 /* PROXY RESPONSE FORMAT
  * 
  * +----+-----+-------+------+----------+----------+
  * |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
  * +----+-----+-------+------+----------+----------+
  * | 1  |  1  | X'00' |  1   | Variable |    2     |
  * +----+-----+-------+------+----------+----------+
  */
  proxyResponse = this->receive(this, 10);
  if(proxyResponse == NULL){
    printf("Error: Failed to establish Socks connection\n");
    return 0; 
  }
    
  if(proxyResponse->data[0] != 5){
    printf("Error: Socks server doesn't think it is version 5");
    return 0; 
  }
  
  if(proxyResponse->data[1] != 0){
    printf("Error: Connection failed");
    return 0; 
  }
  
  proxyResponse->destroyDataContainer(&proxyResponse); 
  
  return 1; 
}

//returns 0 on error
static int ipv4Connect(router* this, char* address, char* port)
{
  struct addrinfo connectionInformation;
  struct addrinfo *encodedAddress;
  struct timeval  recvTimeout;
  
  connectionInformation.ai_family    = AF_INET;
  connectionInformation.ai_socktype  = SOCK_STREAM;
  connectionInformation.ai_flags     = 0;
  connectionInformation.ai_protocol  = 0;
  connectionInformation.ai_canonname = 0;
  connectionInformation.ai_addr      = 0;
  connectionInformation.ai_next      = 0;
  
  if(this->socket != -1){
    printf("Error: Router already in use");
    return 0; 
  }
  
  this->setSocket( this, socket(AF_INET, SOCK_STREAM, 0) );
  if(this->socket == -1){
    printf("Error: Failed to create new socket");
    return 0; 
  } 
  
  //if no data received after 30 seconds (while expecting data) then error out
  recvTimeout.tv_sec  = RECEIVE_WAIT_TIMEOUT_SECONDS; 
  recvTimeout.tv_usec = RECEIVE_WAIT_TIMEOUT_USECS; 
  
  if( setsockopt(this->socket, SOL_SOCKET, SO_RCVTIMEO, &recvTimeout, sizeof(recvTimeout)) ){
    printf("Error: Failed to set receive timeout on socket\n");
    return 0; 
  }
  
 
  if( getaddrinfo(address, port, (const struct addrinfo*)&connectionInformation, &encodedAddress) ){
    printf("Error: Failed to encode address\n");
    return 0; 
  }
  
  if( connect(this->socket, encodedAddress->ai_addr, encodedAddress->ai_addrlen) ){
    printf("Error: Failed to connect to proxy\n");
    return 0; 
  }
  
  return 1; 
}

//return 0 on error
int ipv4Listen(router* this, char* address, int port)
{
  struct sockaddr_in bindInfo;
  struct in_addr     formattedAddress;
  
  if(this->socket != -1){
    printf("Error: Router already in use\n");
    return 0; 
  }
  
  this->socket = socket(AF_INET, SOCK_STREAM, 0);
  if(this->socket == -1){
    printf("Error: Failed to create socket");
    return 0; 
  }
  
  if( !inet_aton( (const char*)address , &formattedAddress) ){
    printf("Error: Failed to convert IP bind address to network order\n"); 
    return 0; 
  }
  
  bindInfo.sin_family       = AF_INET;
  bindInfo.sin_port         = htons(port);
  bindInfo.sin_addr.s_addr  = formattedAddress.s_addr; 
  
  
  if( bind(this->socket, (const struct sockaddr*) &bindInfo, sizeof(bindInfo)) ){
    printf("Error: Failed to bind to address\n");
    return 0; 
  }
  
  if( listen(this->socket, SOMAXCONN) ){
    printf("Error: Failed to listen on socket\n");
    return 0; 
  }
  
  return 1; 
}

//returns -1 on error
static int getConnection(router* this)
{
   if(this->socket == -1){
    printf("Error: Socket not initialized, can't accept\n");
    return -1; 
  }
  
  return accept(this->socket, NULL, NULL); 
}

static void setSocket(router* this, int socket)
{
  this->socket = socket; 
}