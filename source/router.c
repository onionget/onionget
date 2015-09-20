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
#include "dataContainer.h"
#include "memoryManager.h"
#include "ogEnums.h"



//public methods
static dataContainerObject  *receive            ( routerObject *this            , uint32_t payloadBytesize                                                     );
static int                  transmit            ( routerObject *this            , void *payload              , uint32_t payloadBytesize                        );
static int                  socks5Connect       ( routerObject *this            , char *destAddress          , uint8_t destAddressBytesize , uint16_t destPort );
static int                  transmitBytesize    ( routerObject *this            , uint32_t bytesize                                                            );
static uint32_t             getIncomingBytesize ( routerObject *this                                                                                           ); //note that this only gets incoming bytesize if the incoming bytesize is actually sent, as a uint32_t 
static int                  ipv4Connect         ( routerObject *this            , char *ipv4Address          , char *port                                      );
static int                  setSocket           ( routerObject *this            , int socket                                                                   );
static int                  destroyRouter       ( routerObject **thisPointer                                                                                   );
static int                  ipv4Listen          ( routerObject *this            , char *ipv4Address          , int port                                        );
static int                  getConnection       ( routerObject *this                                                                                           );

//private methods
static int socksResponseValidate          ( routerObject *this                                                                                           );
static int sendSocks5ConnectRequest       ( routerObject *this            , char *destAddress          , uint8_t destAddressBytesize , uint16_t destPort );
static int initializeSocks5Protocol       ( routerObject *this                                                                                           );
static int setSocketRecvTimeout           ( routerObject *this            , int timeoutSecs            , int timeoutUsecs                                );






/********** CONSTRUCTOR ****************/

/*
 * newRouter returns a new router object on success and NULL on error.  
 */
routerObject *newRouter(void)
{
  //allocate memory for the router object
  routerObject *this = (routerObject *)secureAllocate(sizeof(*this));
  if(this == NULL){
    printf("Error: Failed to allocate memory for router object\n");
    return NULL;  
  }
  
  //object properties
  this->socket   = -1; 
 
  //object public methods
  this->transmit              = &transmit;
  this->transmitBytesize      = &transmitBytesize;
  this->receive               = &receive;
  this->socks5Connect         = &socks5Connect;
  this->getIncomingBytesize   = &getIncomingBytesize;
  this->ipv4Connect           = &ipv4Connect; 
  this->ipv4Listen            = &ipv4Listen;
  this->getConnection         = &getConnection;
  this->setSocket             = &setSocket; 
  this->destroyRouter         = &destroyRouter;
  
 
  return this; 
}



/********** PUBLIC METHODS ****************/



/*
 * destroyRouter returns 0 on error and 1 on success. It frees the memory associated with the router object. 
 */
static int destroyRouter(routerObject **thisPointer)
{
  routerObject *this;
 
  this = *thisPointer;
  
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been");
    return 0;
  }
  
  if( close(this->socket) != 0 ){
    printf("Error: Failed to close socket\n");
    return 0; 
  }
  
  secureFree(thisPointer, sizeof(**thisPointer)); 
  return 1; 
}


/*
 * recieve returns NULL on error, and a datacontainer containing payloadBytesize of incoming traffic on the socket on success
 */
static dataContainerObject *receive(routerObject *this, uint32_t payloadBytesize)
{
  dataContainerObject   *receivedMessage;
  size_t                bytesReceived;
  size_t                recvReturn;
  
  //basic sanity checks
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been");
    return NULL; 
  }
  
  if(this->socket == -1){
    printf("Error: This router hasn't a valid socket associated with it\n");
    return NULL; 
  }
  
  //allocate the dataContainer object for the incoming message
  receivedMessage = newDataContainer(payloadBytesize);
  if(receivedMessage == NULL){
    printf("Error: Failed to allocate data container for received messager\n");
    return NULL;
  }
    
  for(bytesReceived = 0, recvReturn = 0; bytesReceived != payloadBytesize; bytesReceived += recvReturn){
    recvReturn = recv(this->socket, &(receivedMessage->data[bytesReceived]), payloadBytesize - bytesReceived, 0);     
    if(recvReturn == -1 || recvReturn == 0){ 
      secureFree(&receivedMessage->data, payloadBytesize);
      secureFree(receivedMessage, sizeof(*receivedMessage)); 
      printf("Error: Failed to receive bytes\n");
      return NULL; 
    }     
  }
    
  return receivedMessage;
}

/*
 * getIncomingBytesize receives an incoming uint32_t that encodes the number of subsequent incoming bytes. 
 * NOTE: That this function assumes the interlocutor sends a uint32_t encoding the number of subsequent incoming bytes.
 * 
 * returns 0 on error, note that 0 is also an invalid bytesize for the client to send (TODO: maybe set errno or something) 
 */
static uint32_t getIncomingBytesize(routerObject *this)
{
  uint32_t            incomingBytesize;
  dataContainerObject *incomingBytesizeContainer;
  
  //basic sanity check
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }
  
  //receive the number of incoming bytes, which is encoded as a uint32_t
  incomingBytesizeContainer = this->receive(this, sizeof(uint32_t));
  if(incomingBytesizeContainer == NULL){
    printf("Error: Failed to get incoming bytesize\n");
    return 0;  
  }
  
  //properly encode the number of incoming bytes
  incomingBytesize = *((uint32_t*)incomingBytesizeContainer->data);
  
  //destroy the dataContainer holding the traffic we just received from the network
  if( !incomingBytesizeContainer->destroyDataContainer(&incomingBytesizeContainer) ){
    printf("Error: Failed to destroy data container\n");
    return 0; 
  }
  
  //return the host encoded incoming bytesize 
  return ntohl(incomingBytesize);
}

/*
 * transmit sends payloadBytesize bytes of the buffer pointed to by payload, returns 0 on error and 1 on success
 */
static int transmit(routerObject *this, void *payload, uint32_t payloadBytesize)
{
  size_t sentBytes;  
  size_t sendReturn;
  
  //basic sanity checking
  if(this == NULL || payload == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  if(this->socket == -1){
    printf("Error: Router hasn't a socket set\n");
    return 0;
  }
  
  for(sentBytes = 0, sendReturn = 0; sentBytes != payloadBytesize; sentBytes += sendReturn){
    sendReturn = send(this->socket, payload, payloadBytesize - sentBytes, 0); //TODO note not currently keeping track of payload position because it is void, think of a way around this
    if(sendReturn == -1){
      printf("Error: Failed to send bytes\n");
      return 0;
    } 
  }
 
 return 1; 
}

/*
 * transmityBytesize returns 0 on error and 1 on success. It sends the network encoded bytesize value (bytesize) over the connected socket.
 * This is intended to be received by the function getIncomingBytesize. 
 */
static int transmitBytesize(routerObject *this, uint32_t bytesize)
{
  uint32_t bytesizeEncoded;
  
  //basic sanity checking
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  if(this->socket == -1){
    printf("Error: Router hasn't a socket set\n");
    return 0; 
  }
  
  //encode the bytesize to network order
  bytesizeEncoded = htonl(bytesize);
  
  //transmit the bytesize over the connected socket
  if( !this->transmit(this, &bytesizeEncoded, sizeof(bytesizeEncoded)) ){
    printf("Error: Failed to transmit bytesize\n");
    return 0; 
  }
  
  return 1; 
}



/*
 * socks5Connect establishes a socks 5 connection to destAddress on destPort. Returns 0 on error and 1 on success. See also
 * initializeSocks5Protocol, sendSocks5ConnectRequest, and socksResponseValidate. reference https://www.ietf.org/rfc/rfc1928.txt
 */
static int socks5Connect(routerObject *this, char *destAddress, uint8_t destAddressBytesize, uint16_t destPort)
{  
  if(this == NULL || destAddress == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  if( this->socket == -1 ){
    printf("Error: No socket established for router\n");
    return 0; 
  }
  
  if( !initializeSocks5Protocol(this) ){
    printf("Error: Failed to initialize socks 5 protocol\n");
    return 0;
  }

  if( !sendSocks5ConnectRequest(this, destAddress, destAddressBytesize, destPort) ){
    printf("Error: Failed to send socks connection request\n");
    return 0;
  }
  
  if( !socksResponseValidate(this) ){
    printf("Error: Failed to establish socks connection\n");
    return 0;    
  }
   
  return 1; 
}


/*
 * ipv4Connect connects the router to address:port by setting the routers socket to the connected socket
 * NOTE: address must be in ipv4 format
 * returns 0 on error and 1 on success
 */
static int ipv4Connect(routerObject *this, char *ipv4Address, char *port)
{
  struct addrinfo connectionInformation;
  struct addrinfo *encodedAddress;
  
  if(this == NULL || ipv4Address == NULL || port == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  if(this->socket != -1){
    printf("Error: Router already in use");
    return 0; 
  }
  
  connectionInformation.ai_family    = AF_INET;
  connectionInformation.ai_socktype  = SOCK_STREAM;
  connectionInformation.ai_flags     = 0;
  connectionInformation.ai_protocol  = 0;
  connectionInformation.ai_canonname = 0;
  connectionInformation.ai_addr      = 0;
  connectionInformation.ai_next      = 0;
  

  if( !this->setSocket( this, socket(AF_INET, SOCK_STREAM, 0) ) ){
    printf("Error: Failed to set socket\n");
    return 0;
  }
  
  if(this->socket == -1){
    printf("Error: Failed to create new socket");
    return 0; 
  } 
  
  if( !setSocketRecvTimeout(this, RECEIVE_WAIT_TIMEOUT_SECONDS, RECEIVE_WAIT_TIMEOUT_USECS) ){
    printf("Error: Failed to configure socket properties\n");
    return 0; 
  }
  
  if( getaddrinfo(ipv4Address, port, (const struct addrinfo*)&connectionInformation, &encodedAddress) ){
    printf("Error: Failed to encode address\n");
    return 0; 
  }
  
  if( connect(this->socket, encodedAddress->ai_addr, encodedAddress->ai_addrlen) ){
    printf("Error: Failed to connect to ipv4 address\n");
    return 0; 
  }
  
  return 1; 
}


/*
 * ipv4Listen puts the router into a listening state by creating a socket bound to ipv4Address:port and listening on it
 * returns 0 on error and 1 on success
 */
int ipv4Listen(routerObject *this, char *ipv4Address, int port)
{
  struct sockaddr_in bindInfo;
  struct in_addr     formattedAddress;
  
  if(this == NULL || ipv4Address == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
  
  if(this->socket != -1){
    printf("Error: Router already in use\n");
    return 0; 
  }
  
  if( !this->setSocket(this, socket(AF_INET, SOCK_STREAM, 0)) ){
    printf("Error: Failed to set socket\n");
    return 0; 
  }

  if(this->socket == -1){
    printf("Error: Failed to create socket\n");
    return 0; 
  }
  
  if( !inet_aton( (const char*)ipv4Address , &formattedAddress) ){
    printf("Error: Failed to convert IP bind address to network order\n"); 
    return 0; 
  }
  
  bindInfo.sin_family      = AF_INET;
  bindInfo.sin_port        = htons(port);
  bindInfo.sin_addr.s_addr = formattedAddress.s_addr; 
  
  
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


/*
 * getConnection gets a connection on the listening socket, which must already be initialized (see ipv4Listen)
 * returns -1 on error and the accepted connection on success
 */
static int getConnection(routerObject *this)
{
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return -1; 
  }
  
  if(this->socket == -1){
    printf("Error: Socket not initialized, can't accept\n");
    return -1; 
  }
  
  return accept(this->socket, NULL, NULL); 
}


/*
 * setSocket sets the routers socket to the argument socket
 * returns 0 on error and 1 on success
 */
static int setSocket(routerObject *this, int socket)
{
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been");
    return 0;
  }
  
  this->socket = socket; 
  return 1;
}



/************ PRIVATE METHODS ******************/

/*
 * setSocketRecvTimeout sets how long the routers socket will idle waiting for incoming network traffic for 
 * (when it is actually expecting incoming network traffic). If the timeout period expires with no new data
 * received from the network (while it is being expected), an error occurs. 
 * 
 * returns 0 on error and 1 on success. 
 */
static int setSocketRecvTimeout(routerObject *this, int timeoutSecs, int timeoutUsecs)
{
  struct timeval  recvTimeout;
  
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }
  
  if(this->socket == -1){
    printf("Error: Router doesn't have a socket\n");
    return 0; 
  }
  
  //if no data received after timeoutsecs + timeoutUsecs (while expecting data) then error out
  recvTimeout.tv_sec  = timeoutSecs;  
  recvTimeout.tv_usec = timeoutUsecs;  
  
  if( setsockopt(this->socket, SOL_SOCKET, SO_RCVTIMEO, &recvTimeout, sizeof(recvTimeout)) ){
    printf("Error: Failed to set receive timeout on socket\n");
    return 0; 
  }
  
  return 1; 
}

/* reference https://www.ietf.org/rfc/rfc1928.txt
 * 
 * initializeSocks5protocol engages in the first part of the socks5 protocol, primarily ensuring the server supports socks 5
 * this function also ensures the server doesn't expect authentication, router.c doesn't currently
 * support socks with authentication. 
 * 
 * returns 1 on success, 0 on error (or failure in that the proxy server doesn't support our requirements). 
 * 
 */
static int initializeSocks5Protocol(routerObject *this)
{ 
  dataContainerObject *proxyResponse;
  
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0;
  }
    
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
  
  if( !proxyResponse->destroyDataContainer(&proxyResponse) ){
    printf("Error: Failed to destroy data container\n");
    return 0;
  }
  
  return 1; 
}

/*
 * sendSocks5ConnectionRequest engages in the second part of the socks5 protocol, primarily attempting to establish a 
 * connection to destAddress:destPort through the socks proxy. reference https://www.ietf.org/rfc/rfc1928.txt
 * 
 * returns 0 on error (or failure), 1 on success. 
 * NOTE: desAddress should be a URL (I don't think this currently supports IP addresses, but TODO I should look into this) 
 */
static int sendSocks5ConnectRequest(routerObject *this, char *destAddress, uint8_t destAddressBytesize, uint16_t destPort)
{
  int                 requestBytesize;
  dataContainerObject *socksRequest;
  
  if(this == NULL || destAddress == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }
  
  //format the destination port in network order
  destPort = htons(destPort);
  
  
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
  
  if( !socksRequest->destroyDataContainer(&socksRequest) ){
    printf("Error: Failed to destroy data container\n");
    return 0; 
  }
  
  return 1; 
}


/*
 * socksResponseValidate gets the final response from the socks server and ensures that everything has gone correctly.
 * reference https://www.ietf.org/rfc/rfc1928.txt
 * 
 * returns 0 on error (or failure) and 1 on success. 
 * 
 */
static int socksResponseValidate(routerObject *this)
{
  dataContainerObject *proxyResponse;
  
  if(this == NULL){
    printf("Error: Something was NULL that shouldn't have been\n");
    return 0; 
  }
  
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
  
  if( !proxyResponse->destroyDataContainer(&proxyResponse) ){
    printf("Error: Failed to destroy datacontainer\n");
    return 0;
  }
  
  return 1; 
}