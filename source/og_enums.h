//controller

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

enum{  DELIMITER_BYTESIZE     = 1   };
enum{  FILE_CHUNK_BYTESIZE    = 1000000 }; 


//diskfile

enum{ COUNT = 1 };



//dlinkedlist
enum{ DLL_HEAD = 0 };
enum{ DLL_TAIL = 1 };


//router
enum{ RECEIVE_WAIT_TIMEOUT_SECONDS = 30 };
enum{ RECEIVE_WAIT_TIMEOUT_USECS   = 0  };