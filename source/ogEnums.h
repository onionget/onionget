//controller

#pragma once

enum{ HIGHEST_VALID_PORT      = 65535}; 

enum{ EXE_NAME                = 0 }; 
enum{ CLIENT_OR_SERVER        = 1 };

enum{ S_FIXED_CLI_INPUTS      = 6 };

enum{ S_BIND_ADDRESS          = 2 };
enum{ S_LISTEN_PORT           = 3 };
enum{ S_DIR_PATH              = 4 };
enum{ S_MEM_MEGA_CACHE        = 5 }; 

enum{ C_FIXED_CLI_INPUTS      = 8 };

enum{ C_TOR_BIND_ADDRESS      = 2 };
enum{ C_TOR_PORT              = 3 };
enum{ C_ONION_ADDRESS         = 4 };
enum{ C_ONION_PORT            = 5 };
enum{ C_OPERATION             = 6 };
enum{ C_DIR_PATH              = 7 };
enum{ C_FIRST_FILE_NAME       = 8 };

enum{ C_VALID_OPERATION_COUNT = 1 };


enum{  ONION_ADDRESS_BYTESIZE = 22  };


//client

//NOTE https://en.wikipedia.org/wiki/Page_%28computer_memory%29 
//NOTE that for mmap to work (for writing files, and also reading) this must be a multiple of 
//which is typically 4096, or 8192, this file chunk bytesize supports: 4kib, 8kib, 64kib, 256kib, 2mib page sizes
//providing support for all standard page sizes, as well as many huge page sizes (only in PAE mode for 32bit x86). 
//TODO add support for arbitrary page sizes
enum{  FILE_CHUNK_BYTESIZE    = 2097152 }; 


//diskfile

enum{ COUNT = 1 };
enum{ FILE_START = 0}; 


//dlinkedlist
enum{ DLL_HEAD = 0 };
enum{ DLL_TAIL = 1 };


//router
enum{ RECEIVE_WAIT_TIMEOUT_SECONDS = 30 };
enum{ RECEIVE_WAIT_TIMEOUT_USECS   = 0  };


//server

enum{  TERMINATING_NULL_HOP        = 1         };
enum{  MAX_REQUEST_STRING_BYTESIZE = 1000000   };
enum{  BYTES_IN_A_MEGABYTE         = 1000000   }; 
enum{  MAX_FILE_ID_BYTESIZE        = 200       }; //todo make this saner