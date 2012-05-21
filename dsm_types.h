
#ifndef _DSM_TYPES_H_
#define _DSM_TYPES_H_

#include <features.h>
#include <endian.h>
#include <sched.h>
#include <time.h>

#include <bits/pthreadtypes.h>

typedef int             int32;
typedef unsigned int    uInt32;
typedef char            int8;
typedef unsigned char   uInt8;
typedef short           int16;
typedef unsigned short  uInt16;

typedef enum {
    DSM_MSG_INIT_SHARED_REGION_REQ,
    DSM_MSG_INIT_SHARED_REGION_RSP,
    DSM_MSG_PAGE_REQ,
    DSM_MSG_PAGE_RSP
}dsmMsgType;

typedef enum {
    DSM_TRACE_TYPE_INFO,
    DSM_TRACE_TYPE_ERROR,
    DSM_TRACE_TYPE_WARN,
    DSM_TRACE_TYPE_DEBUG
}dsmTraceType;

typedef struct {
    /* type of msg */
    dsmMsgType      msgType;
    /* length of msg */
    uInt32          payloadLen;
    /* strechable array for msg payload */
    uInt8           payload[1];

}dsmMsg;

typedef struct {
    int32   isMaster;
    char*   mIpAddr;
    int32   mPort;
    char*   oIpAddr;
    int32   oPort;
    uInt32  numPagesToAlloc;
}dsmMapInitInfo;

typedef struct {
    int32   serverSd;           /* socket fd to listen to req from peer */
    int32   reqSockSd;          /* socket fd to send req to peer */
    int32   currentClientSd;    /* socket fd of current client */
}dsmSocketInfo;

typedef enum {
    DSM_MAIN_THREAD,
    DSM_COMMUNICATION_THREAD
}dsmThreadType;

typedef enum {
    DSM_PAGE_REQUESTED = 1, // the page is requested from the owner
    DSM_PAGE_IN_TRANSFER,   // the page is currently getting transferred from the owner
    DSM_PAGE_PRESENT,       // the page is present at the current location
    DSM_PAGE_NOT_PRESENT    // the page is not present at the current location
}dsmPageStatus;

typedef struct {
    bool                    owner;
    dsmPageStatus           pageStatus;
    pthread_mutex_t         pteMutexVar;
    pthread_cond_t          pteCondVar;
}dsmPageTableEntry;



#endif

