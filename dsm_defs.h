
#ifndef _DSM_INIT_H_
#define _DSM_INIT_H_


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include "dsm_types.h"


#define DSM_MAX_THREADS             (2)
#define DSM_MAX_IP_ADDR_LEN         (16)
#define DSM_PAGE_SIZE               (4096)
#define DSM_DEF_PAGE_SIZE           (4096)
#define DSM_MAX_PAGE_TABLE_ENTRY    (50000)
#define DSM_MSG_HDR_LEN             (8)
#define DSM_MAX_MSG_LEN             (DSM_PAGE_SIZE + DSM_MSG_HDR_LEN + sizeof(uInt32))

extern void*                pDsmSharedRegion;
extern int*                 pDsmMasterInitAddr;
extern dsmSocketInfo        dsmSockInfo;
extern dsmMapInitInfo       dsmMmapInfo;
extern dsmPageTableEntry    dsmPageTable[DSM_MAX_PAGE_TABLE_ENTRY];


#ifdef DSM_ENABLE_LOG
#define dsmPrintLog(type, ...) \
{\
    switch(type) {\
        case DSM_TRACE_TYPE_INFO:\
            printf("[INFO]::[%s:%d]::", __func__, __LINE__);\
            break;\
        case DSM_TRACE_TYPE_ERROR:\
            printf("[ERROR]::[%s:%d]::", __func__, __LINE__);\
            break;\
        case DSM_TRACE_TYPE_WARN:\
            printf("[WARN]::[%s:%d]::", __func__, __LINE__);\
            break;\
        case DSM_TRACE_TYPE_DEBUG:\
            printf("[DEBUG]::[%s:%d]::", __func__, __LINE__);\
            break;\
    }\
    dsmPrintf(__VA_ARGS__);\
}

#define dsmEnterFunc() printf("[DEBUG]::Entering Function [%s]\n", __func__);

#define dsmExitFunc() printf("[DEBUG]::Exiting function [%s:%d]\n", __func__, __LINE__);

#else
#define dsmPrintLog(type, ...) ;
#define dsmEnterFunc() ;
#define dsmExitFunc() ;
#endif




#endif

