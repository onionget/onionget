#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/capability.h>
#include <sys/mman.h> //don't think this is needed actually TODO
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "systemManager.h"


static int enableCapability(cap_value_t *capability, cap_flag_t flag);
static int disableCapability(cap_value_t *capability, cap_flag_t flag);
static int setCapability(cap_value_t *capability, cap_flag_t flag, cap_flag_value_t value);

static int enableIsolateFs(void);
static int disableIsolateFs(void);


/*
 * FIO16-C suggests using chroot to isolate processes from the wider filesystem, this code provides a solution for chrooting the process
 * MEM06-C suggests not allowing sensitive data to be written to disk, this requires mlock, this code allows enabling mlock (and preventing core dumps)
 * 
 * Note that chroot as a security technique is highly controversially and that SELinux and/or similar MAC/RBAC profiles should be 
 * used for security via isolation, however I don't believe that using chroot in addition to this will cause any problems, and perhaps
 * it will harden things up to an extent, some people (including cert!) argue that it does.
 * 
 * these solutions require libcap-dev, -lcap, and for the binary to be given capabilities as follows:
 * 
 * sudo setcap cap_sys_chroot,cap_ipc_lock=+p  /path/to/executable
 * 
 * TODO make a bash compilation script that automatically sets the appropriate binary capabilities 
 */


/***functions related to core dump ******/
int disableCoreDumps(void)
{
  struct rlimit limit; 
  limit.rlim_cur = 0;
  limit.rlim_max = 0;
  if(setrlimit(RLIMIT_CORE, &limit) != 0){
    printf("Error: Failed to disable core dumps\n");
    return 0; 
  } 
  
  return 1; 
}


/*** Functions related to capabilities ****/

/*
 * enableCapability sets capability to have flag. It returns 0 on error and 1 on success. 
 */
static int enableCapability(cap_value_t *capability, cap_flag_t flag)
{
  if( !setCapability(capability, flag, CAP_SET) ){
    return 0; 
  }
  return 1; 
}


/*
 * disableCapability clears flag from capability. It returns 0 on error and 1 on success. 
 */
static int disableCapability(cap_value_t *capability, cap_flag_t flag)
{
  if( !setCapability(capability, flag, CAP_CLEAR) ){
    return 0;
  }
  return 1; 
}


static int setCapability(cap_value_t *capability, cap_flag_t flag, cap_flag_value_t value)
{
  cap_t currentCapabilities = NULL;
  
  //get the current process capabilities
  currentCapabilities = cap_get_proc();
  if(currentCapabilities == NULL){
    printf("Error: Failed to determine current process capabilities\n");
    return 0; 
  }
  
  //add the new process capability to the list of current process capabilities
  if (cap_set_flag(currentCapabilities, flag, 1, capability, value) == -1){
    printf("Error: Failed to add new process capabilities to the list of process capabilities\n");
    cap_free(currentCapabilities);
    return 0;
  }
  
  //set the process to use the new capability
  if( cap_set_proc(currentCapabilities) == -1 ){
    printf("Error: Failed to set the process to use required new capabilities\n");
    cap_free(currentCapabilities);
    return 0; 
  }
  
    //clean up 
  if( cap_free(currentCapabilities) == -1 ){
    printf("Error: Failed to free memory associated with process capabilities\n");
    return 0; 
  }
  
  return 1;
}



/*** functions related to memory locking  ****/ 


/*
 * enableMlock allows the process to use mlock, it returns 0 on failure and 1 on success. It may be called once per process. 
 */
int enableMlock(void)
{
  cap_value_t capability[1] = {CAP_IPC_LOCK};
  static int alreadyCalled  = 0;
  
  if(alreadyCalled){
    printf("Error: You can only enable mlock once per process\n");
    return 0; 
  }
  
  //make sure the capability of CAP_IPC_LOCK is available (required for mlock) 
  if( !CAP_IS_SUPPORTED(CAP_IPC_LOCK) ){
    printf("Error: Mlock capability [CAP_IPC_LOCK] isn't set on binary\n");
    return 0; 
  }
  
  if( !enableCapability(capability, CAP_EFFECTIVE) ){
    printf("Error: Failed to make process chroot capable\n");
    return 0; 
  }
  
  alreadyCalled = 1; 
  
  return 1; 
}


/*
 * disableMlock removes the processes ability to use mlock, it returns 0 on failure and 1 on success. 
 */
int disableMlock(void)
{
  cap_value_t capability[1] = {CAP_IPC_LOCK};
  
  if( !disableCapability(capability, CAP_EFFECTIVE) ){
    printf("Error: Failed to diable mLock capability\n");
    return 0;
  }
  
  return 1; 
}




/*** Functions related to file system isolation (using chroot) ****/ 


/*
 * isolateFs changes the root directory (chroots) to jailPath, it returns 1 on success and 0 on error. It can only be called once per function,
 * as is enforced by its call to enableIsolateFs. The capability is automatically disabled after utilized. 
 */
int isolateFs(const char *jailPath)
{
  if( !enableIsolateFs() ){
    printf("Error: Failed to enable the ability to isolate the filesystem with chroot\n");
    return 0; 
  }
  
  //initialize the chroot jail, chdir to chroot in compliance with POS05-C / FIO16-C
  if( chroot(jailPath) == -1 || chdir("/") == -1 ){
    printf("Error: Failed to isolate file system!\n");
    return 0; 
  }
  
  if( !disableIsolateFs() ){
    printf("Error: Failed to disable the ability to isolate the filesystem with chroot\n");
    return 0; 
  }
  
  return 1; 
}



/*
 * enableIsolateFs allows the process to utilize chroot. It returns 1 on success and 0 on error. It may be called once per process. 
 */
static int enableIsolateFs(void)
{  
  cap_value_t capability[1] = {CAP_SYS_CHROOT};
  static int  alreadyCalled = 0;
  
  if(alreadyCalled){
    printf("Error: You can only enable isolate Fs once per process\n");
    return 0; 
  }
    
  //make sure that the capability of chroot is available
  if( !CAP_IS_SUPPORTED(CAP_SYS_CHROOT) ){
    printf("Error: Chroot capability [CAP_SYS_CHROOT] isn't set on binary\n");
    return 0; 
  }
  
  if( !enableCapability(capability, CAP_EFFECTIVE) ){
    printf("Error: Failed to make process chroot capable\n");
    return 0; 
  }
  
  alreadyCalled = 1; 
    
  return 1; 
}



/*
 * disableIsolateFs removes the chroot capability of the process
 */
static int disableIsolateFs(void)
{
  cap_value_t capability[1] = {CAP_SYS_CHROOT};
    
  if( !disableCapability(capability, CAP_EFFECTIVE) ){
    printf("Error: Failed to disable chroot property\n");
    return 0; 
  }
  
  return 1; 
}