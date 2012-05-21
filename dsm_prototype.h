
#ifndef _DSM_PROTOTYPE_H_
#define _DSM_PROTOTYPE_H_

#include "dsm.h"

/* init functions */
int dsmThreadInit(int, char*, int, char*, int, unsigned);
void* dsmSharedMemoryInit(void*);
void* dsmCreateSharedRegion(dsmMapInitInfo);
void initializeDSM(int, char*, int, char, int, unsigned);
void* getsharedregion(void);


/* comm functions */
int dsmOpenSocket(char*, int );
int dsmCreateSocket(void);
void* dsmAcceptAndRead(void*);
int dsmConnectToPeer(int, char*, int);
int dsmSendMsg(int, dsmMsg*);
int dsmRecvMsg(int);


/* msg functions */
void dsmDecodeMsg(void*);
int dsmInitSharedRegionReqHandler(void*);
int dsmInitSharedRegionRspHandler(void*);
int dsmPageReqHandler(void*);
int dsmPageRspHandler(void*);
void dsmPageFaultHandler(int, siginfo_t*, void*);
    
/* util functions */
void dsmPrintf(const char *format, ...);

#endif

