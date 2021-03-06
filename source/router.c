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
#include "memoryManager.h"
#include "ogEnums.h"
#include "macros.h"

//private internal values 
typedef struct routerPrivate{
  routerObject   publicRouter;
  int            socket; 
}routerPrivate;


//public methods
static int                  receive             ( routerObject *this            , void *receiveBuffer        , uint32_t payloadBytesize                        );
static int                  transmit            ( routerObject *this            , void *payload              , uint32_t payloadBytesize                        );
static int                  socks5Connect       ( routerObject *this            , char *destAddress          , uint8_t destAddressBytesize , uint16_t destPort );
static int                  transmitBytesize    ( routerObject *this            , uint32_t bytesize                                                            );
static uint32_t             getIncomingBytesize ( routerObject *this                                                                                           ); //note that this only gets incoming bytesize if the incoming bytesize is actually sent, as a uint32_t 
static int                  ipv4Connect         ( routerObject *this            , char *ipv4Address          , char *port                                      );
static int                  setSocket           ( routerObject *this            , int socket                                                                   );
static int                  destroyRouter       ( routerObject **thisPointer                                                                                   );
static int                  ipv4Listen          ( routerObject *this            , char *ipv4Address          , int port                                        );
static int                  getConnection       ( routerObject *this                                                                                           );
static int                  reinitialize        ( routerObject *this                                                                                           ); 

//private methods
static int socksResponseValidate          ( routerObject  *this                                                                                                  );
static int sendSocks5ConnectRequest       ( routerObject  *this                   , char *destAddress          , uint8_t destAddressBytesize , uint16_t destPort );
static int initializeSocks5Protocol       ( routerObject  *this                                                                                                  );
static int setSocketRecvTimeout           ( routerObject  *this                   , int timeoutSecs            , int timeoutUsecs                                );

//TODO add reinitialize function (close socket and reset to -1); 




/********** CONSTRUCTOR ****************/

/*
 * newRouter returns a new router object on success and NULL on error.  
 */
routerObject *newRouter(void)
{
  routerPrivate *privateThis = NULL;
  
  //allocate memory for the object
  privateThis = (routerPrivate *) secureAllocate(sizeof(*privateThis)); 
  if(privateThis == NULL){
    logEvent("Error", "Failed to allocate memory for router object");
    return NULL;  
  }
  
 
  //initialize public methods
  privateThis->publicRouter.transmit              = &transmit;
  privateThis->publicRouter.transmitBytesize      = &transmitBytesize;
  privateThis->publicRouter.receive               = &receive;
  privateThis->publicRouter.socks5Connect         = &socks5Connect;
  privateThis->publicRouter.getIncomingBytesize   = &getIncomingBytesize;
  privateThis->publicRouter.ipv4Connect           = &ipv4Connect; 
  privateThis->publicRouter.ipv4Listen            = &ipv4Listen;
  privateThis->publicRouter.getConnection         = &getConnection;
  privateThis->publicRouter.setSocket             = &setSocket; 
  privateThis->publicRouter.destroyRouter         = &destroyRouter;
  privateThis->publicRouter.reinitialize          = &reinitialize; 
  
  
  //initialize private properties
  privateThis->socket   = -1; 
  
 
  return (routerObject *) privateThis; 
}



/********** PUBLIC METHODS ****************/


/*
 * reinitialize closes the current socket and resets the router to a new state
 */
static int reinitialize(routerObject *this)
{
  routerPrivate *private = (routerPrivate *)this;
  
  if(this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  if(private->socket != -1 && close(private->socket) != 0){
    logEvent("Error", "Failed to close socket, router reinitialization failed\n");
    return 0;
  }
  
  private->socket = -1; 
  
  return 1;   
}


/*
 * destroyRouter returns 0 on error and 1 on success. It closes the socket (if open) and frees the memory associated with the router object. 
 */
static int destroyRouter(routerObject **thisPointer)
{
  routerPrivate **privateThisPointer = NULL;
  routerPrivate *privateThis         = NULL;
  
  privateThisPointer = (routerPrivate **)thisPointer;
  privateThis        = *privateThisPointer; 
    
  if(privateThis == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  if( privateThis->socket != -1 && close(privateThis->socket) != 0 ){
    logEvent("Error", "Failed to close socket");
    return 0; 
  }
  
  secureFree(privateThisPointer, sizeof(routerPrivate)); 
  return 1; 
}


/*
 * recieve returns 0 on error, 1 on success
 */
static int receive(routerObject *this, void *receiveBuffer, uint32_t payloadBytesize)
{
  routerPrivate         *private         = NULL;
  size_t                bytesReceived    = 0;
  size_t                recvReturn       = 0;
  
  private = (routerPrivate *)this; 
  
  //basic sanity checks
  if(private == NULL || this == NULL){ //this is never NULL if private is but keeping it like it is for now for readability
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0; 
  }
  
  if(private->socket == -1){
    logEvent("Error", "This router hasn't a valid socket associated with it");
    return 0; 
  }
    
  for(bytesReceived = 0, recvReturn = 0; bytesReceived != payloadBytesize; bytesReceived += recvReturn){
    recvReturn = recv(private->socket, &((unsigned char*)receiveBuffer)[bytesReceived], payloadBytesize - bytesReceived, 0);    //TODO look into this cast from void* 
    if(recvReturn == -1 || recvReturn == 0){ 
      logEvent("Error", "Failed to receive bytes");
      return 0; 
    }     
  }
    
  return 1;
}

/*
 * getIncomingBytesize receives an incoming uint32_t that encodes the number of subsequent incoming bytes. 
 * NOTE: That this function assumes the interlocutor sends a uint32_t encoding the number of subsequent incoming bytes.
 * 
 * returns 0 on error, note that 0 is also an invalid bytesize for the client to send (TODO: better error checking coming soon) 
 */
static uint32_t getIncomingBytesize(routerObject *this)
{
  uint32_t incomingBytesize = 0;

  //basic sanity check
  if(this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0; 
  }
  
  //receive the number of incoming bytes, which is encoded as a uint32_t
  if( !this->receive(this, &incomingBytesize, sizeof(uint32_t)) ){
    logEvent("Error", "Failed to receive incoming bytesize");
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
  size_t        sentBytes    = 0;  
  size_t        sendReturn   = 0;
  routerPrivate *private     = NULL; 
  
  private = (routerPrivate *)this; 
  
  //basic sanity checking
  if(private == NULL || payload == NULL || this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  if(private->socket == -1){
    logEvent("Error", "Router hasn't a socket set");
    return 0;
  }
  
  for(sentBytes = 0, sendReturn = 0; sentBytes != payloadBytesize; sentBytes += sendReturn){
    sendReturn = send(private->socket, payload, payloadBytesize - sentBytes, 0); //TODO note not currently keeping track of payload position because it is void, think of a way around this
    if(sendReturn == -1){
      logEvent("Error", "Failed to send bytes");
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
  uint32_t       bytesizeEncoded = 0;
  routerPrivate  *private        = NULL; 
  
  private = (routerPrivate *)this; 
  
  //basic sanity checking
  if(private == NULL || this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  if(private->socket == -1){
    logEvent("Error", "Router hasn't a socket set");
    return 0; 
  }
  
  //encode the bytesize to network order
  bytesizeEncoded = htonl(bytesize);
  
  //transmit the bytesize over the connected socket
  if( !this->transmit(this, &bytesizeEncoded, sizeof(bytesizeEncoded)) ){
    logEvent("Error", "Failed to transmit bytesize");
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
  
  routerPrivate *private = NULL; 
  
  private = (routerPrivate *)this; 
  
  if(private == NULL || destAddress == NULL || this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  if( private->socket == -1 ){
    logEvent("Error", "No socket established for router");
    return 0; 
  }
  
  if( !initializeSocks5Protocol(this) ){
    logEvent("Error", "Failed to initialize socks 5 protocol");
    return 0;
  }

  if( !sendSocks5ConnectRequest(this, destAddress, destAddressBytesize, destPort) ){
    logEvent("Error", "Failed to send socks connection request");
    return 0;
  }
  
  if( !socksResponseValidate(this) ){
    logEvent("Error", "Failed to establish socks connection");
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
  
  routerPrivate *private = NULL; 
  
  private = (routerPrivate *)this;
  
  if(private == NULL || this == NULL || ipv4Address == NULL || port == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  if(private->socket != -1){
    logEvent("Error", "Router already in use");
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
    logEvent("Error", "Failed to set socket");
    return 0;
  }
  
  if(private->socket == -1){
    logEvent("Error", "Failed to create new socket");
    return 0; 
  } 
  
  if( !setSocketRecvTimeout(this, RECEIVE_WAIT_TIMEOUT_SECONDS, RECEIVE_WAIT_TIMEOUT_USECS) ){
    logEvent("Error", "Failed to configure socket properties");
    return 0; 
  }
  
  if( getaddrinfo(ipv4Address, port, (const struct addrinfo*)&connectionInformation, &encodedAddress) ){
    logEvent("Error", "Failed to encode address");
    return 0; 
  }
  
  if( connect(private->socket, encodedAddress->ai_addr, encodedAddress->ai_addrlen) ){
    logEvent("Error", "Failed to connect to ipv4 address");
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
  
  routerPrivate *private = NULL; 
  private                = (routerPrivate *)this;
  
  if(private == NULL || this == NULL || ipv4Address == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  if(private->socket != -1){
    logEvent("Error", "Router already in use");
    return 0; 
  }
  
  if( !this->setSocket(this, socket(AF_INET, SOCK_STREAM, 0)) ){
    logEvent("Error", "Failed to set socket");
    return 0; 
  }

  if(private->socket == -1){
    logEvent("Error", "Failed to create socket");
    return 0; 
  }
  
  if( !inet_aton( (const char*)ipv4Address , &formattedAddress) ){
    logEvent("Error", "Failed to convert IP bind address to network order"); 
    return 0; 
  }
  
  bindInfo.sin_family      = AF_INET;
  bindInfo.sin_port        = htons(port);
  bindInfo.sin_addr.s_addr = formattedAddress.s_addr; 
  
  
  if( bind(private->socket, (const struct sockaddr*) &bindInfo, sizeof(bindInfo)) ){
    logEvent("Error", "Failed to bind to address");
    return 0; 
  }
  
  if( listen(private->socket, SOMAXCONN) ){
    logEvent("Error", "Failed to listen on socket");
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
  routerPrivate *private = NULL; 
  private                = (routerPrivate *)this;
  
  if(private == NULL || this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return -1; 
  }
  
  if(private->socket == -1){
    logEvent("Error", "Socket not initialized, can't accept");
    return -1; 
  }
  
  return accept(private->socket, NULL, NULL); 
}


/*
 * setSocket sets the routers socket to the argument socket
 * returns 0 on error and 1 on success
 */
static int setSocket(routerObject *this, int socket)
{
  routerPrivate *private = NULL; 
  private                = (routerPrivate *)this;
  
  if(private == NULL || this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0;
  }
  
  private->socket = socket; 
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
  
  routerPrivate *private = NULL; 
  private                = (routerPrivate *)this;
  
  if(private == NULL || this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    return 0; 
  }
  
  if(private->socket == -1){
    logEvent("Error", "Router doesn't have a socket");
    return 0; 
  }
  
  //if no data received after timeoutsecs + timeoutUsecs (while expecting data) then error out
  recvTimeout.tv_sec  = timeoutSecs;  
  recvTimeout.tv_usec = timeoutUsecs;  
  
  if( setsockopt(private->socket, SOL_SOCKET, SO_RCVTIMEO, &recvTimeout, sizeof(recvTimeout)) ){
    logEvent("Error", "Failed to set receive timeout on socket");
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
  char proxyResponse[2]; 
  
  if(this == NULL){
    memoryClear(proxyResponse, 2);
    logEvent("Error", "Something was NULL that shouldn't have been"); 
    return 0;
  }
    
  // | VER | NMETHODS | METHODS |
  //socks version 5, one method, no authentication
  if( !this->transmit(this, "\005\001\000", 3) ){
    memoryClear(proxyResponse, 2);
    logEvent("Error", "Socks connection failed to transmit bytes to socks server");
    return 0;
  }
  
  // | VER | METHOD |
  if( !this->receive(this, &proxyResponse, 2) ){
    memoryClear(proxyResponse, 2);
    logEvent("Error", "Failed to get response from proxy");
    return 0; 
  }

  
  if(proxyResponse[0] != 5){
    memoryClear(proxyResponse, 2);
    logEvent("Error", "Proxy doesn't think it is socks 5");
    return 0; 
  }
  
  if(proxyResponse[1] != 0){
    memoryClear(proxyResponse, 2);
    logEvent("Error", "Proxy expects authentication");
    return 0; 
  }
  
  memoryClear(proxyResponse, 2);
  return 1; 
}

/*
 * sendSocks5ConnectionRequest engages in the second part of the socks5 protocol, primarily attempting to establish a 
 * connection to destAddress:destPort through the socks proxy. reference https://www.ietf.org/rfc/rfc1928.txt
 * 
 * returns 0 on error (or failure), 1 on success. 
 * NOTE: desAddress should be a URL (I don't think this currently supports IP addresses, but TODO I should look into this) 
 */

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
  */ //TODO wipe stack when done look at memory manager to make sure can do this
static int sendSocks5ConnectRequest(routerObject *this, char *destAddress, uint8_t destAddressBytesize, uint16_t destPort)
{
  int fixedSocksBytes        = 1 + 1 + 1 + 1 + 1 + 2;
  int maxDestAddressBytesize = 100; 
 
  char socksRequest[fixedSocksBytes + maxDestAddressBytesize];
  
  int requestBytesize = fixedSocksBytes + destAddressBytesize; //TODO look into cast differences, make sure it is legit
  
  if(requestBytesize > fixedSocksBytes + maxDestAddressBytesize){
    logEvent("Error", "Dest address bytesize must be 100 or less");
    memoryClear(socksRequest, fixedSocksBytes + maxDestAddressBytesize);
    return 0; 
  }
  
  if(this == NULL || destAddress == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
    memoryClear(socksRequest, fixedSocksBytes + maxDestAddressBytesize);
    return 0; 
  }
  
  //format the destination port in network order
  destPort = htons(destPort);
  
    
  //first four bytes: socks version 5, command is connect, rsv is null, identifier is domain name 
  memcpy(&socksRequest[0], "\005\001\000\003", 4);
  
  //fifth byte: prepend identifier with its bytesize 
  memcpy(&socksRequest[4], &destAddressBytesize, 1);
  
  //variable bytes: the destination identifier
  memcpy(&socksRequest[5], destAddress, destAddressBytesize);
  
  //final two bytes: the destination port in network octet order
  memcpy(&socksRequest[5 + destAddressBytesize], &destPort, 2);
  
  
  if( !this->transmit(this, socksRequest, requestBytesize) ){
    memoryClear(socksRequest, fixedSocksBytes + maxDestAddressBytesize); //TODO error checking that doesn't suck ass soon
    logEvent("Error", "Failed to transmit message to socks server");
    return 0;
  }
  
  memoryClear(socksRequest, fixedSocksBytes + maxDestAddressBytesize);
  
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
  char proxyResponse[10];
  
  if(this == NULL){
    logEvent("Error", "Something was NULL that shouldn't have been");
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
  if( !this->receive(this, &proxyResponse, 10) ){
    memoryClear(proxyResponse, 10);
    logEvent("Error", "Failed to receive proxy response");
    return 0; 
  }

  if(proxyResponse[0] != 5){
    memoryClear(proxyResponse, 10);
    logEvent("Error", "Socks server doesn't think it is version 5"); //todo better error checking soon
    return 0; 
  }
  
  if(proxyResponse[1] != 0){
    memoryClear(proxyResponse, 10);
    logEvent("Error", "Connection failed");
    return 0; 
  }
   
  memoryClear(proxyResponse, 10);
  return 1; 
}