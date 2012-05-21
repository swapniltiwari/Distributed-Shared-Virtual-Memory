
#include "dsm_types.h"
#include "dsm_defs.h"
#include "dsm_socket.h"
#include "dsm_prototype.h"

/* Global definitions */
void*               pDsmSharedRegion = NULL;
int32*                pDsmMasterInitAddr = NULL;
dsmMapInitInfo      dsmMmapInfo;
dsmPageTableEntry   dsmPageTable[DSM_MAX_PAGE_TABLE_ENTRY];

/*
 * Creates a new shared region depending on the given parameters
 * At Master :     Creates a shared region with write enabled permissions
 * At Client :     Creates a shared region with base address received from
 *                 Master, and no read/write permissions
 */
void* dsmCreateSharedRegion()
{
    int             pageSize = -1;

    dsmEnterFunc();
    pageSize = sysconf(_SC_PAGE_SIZE);
    if (pageSize == -1) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Failed to retrieve system page size. "
                "Errno: [%d]\n", errno);
        /* Continuing assuming the default page size */
        dsmPrintLog(DSM_TRACE_TYPE_WARN, "Continuing with default page size of 4KB\n");
        pageSize = DSM_DEF_PAGE_SIZE;
    }

    if (dsmMmapInfo.isMaster) {
        pDsmSharedRegion = mmap((void*)NULL, (dsmMmapInfo.numPagesToAlloc * pageSize),
                PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    }
    else {
        pDsmSharedRegion = mmap((void*)pDsmMasterInitAddr, (dsmMmapInfo.numPagesToAlloc * pageSize),
                PROT_NONE, MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
    }

    if (NULL == pDsmSharedRegion) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Shared memory region creation failed\n");
        dsmExitFunc();
        abort();
    }
    else {
       dsmPrintLog(DSM_TRACE_TYPE_INFO, "Shared memory region created with "
          "base address : %p\n", pDsmSharedRegion);
    }
    dsmExitFunc();
}

/*
 * Initializes shared memory region on master
 * waits for shared region base addr in slave; on receiving base addr create 
 * shared memory region on client 
 * Returns 0 on success, aborts on error
 */
int32 dsmSharedMemoryInit()
{
    int32     retval = -1;

    dsmEnterFunc();
    /* if master create shared region;
     * else send msg to master for base addr of shared region and wait for response
     * create shared memory region when response is rcvd from master */
    if (dsmMmapInfo.isMaster) {
        dsmCreateSharedRegion();
    }
    else {
        /* client waits for master to be up */
        do {
            /* Connect to the Master for Shared region base address */
            usleep(100);
            retval = dsmConnectToPeer(dsmSockInfo.reqSockSd, dsmMmapInfo.mIpAddr,
                    dsmMmapInfo.mPort);
            /* Abort if the network is unreachable */
            if (errno == ENETUNREACH) {
                dsmPrintLog(DSM_TRACE_TYPE_ERROR,"errno: [%d], Error Desc: "
                        "[The network isn't reachable from this host]\n Aborting ...\n", errno);
                dsmExitFunc();
                abort();
            }
        } while (-1 == retval);

        /* request for shared region base address */
        dsmMsg msg;
        memset(&msg, 0, sizeof(dsmMsg)); 
        msg.msgType = DSM_MSG_INIT_SHARED_REGION_REQ;
        msg.payloadLen = 0;

        retval = dsmSendMsg(dsmSockInfo.reqSockSd, &msg);
        if (-1 == retval) {
            dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Error sending message, Please check below for description \n");
            if (errno == EINTR) {
                dsmPrintLog(DSM_TRACE_TYPE_ERROR, "errno: [%d], Error Desc: "
                        "[Operation interrupted by a signal before completion]\n", errno);
                close(dsmSockInfo.reqSockSd);
                abort();
            }
            else if (errno == EPIPE) {
                dsmPrintLog(DSM_TRACE_TYPE_ERROR, "errno: [%d], Error Desc: "
                        "[The other side closed the connection]\n Aborting ...\n", errno);
                close(dsmSockInfo.reqSockSd);
                abort();
            }
        }

        retval = dsmRecvMsg(dsmSockInfo.reqSockSd);
        if (-1 == retval) {
            dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Error receiving message!\n");
            if (errno == EINTR) {
                dsmPrintLog(DSM_TRACE_TYPE_ERROR, "errno: [%d], Error Desc: "
                        "[Operation interrupted by a signal before completion]\n", errno);
                close(dsmSockInfo.reqSockSd);
                abort();
            }
        }

        while ((int32*)0 == pDsmMasterInitAddr) {
            usleep(100);
        }
        dsmCreateSharedRegion();
    }

    dsmExitFunc();
    return 0;
}

/*
 * 1. initializes sockets
 * 2. spawns communication thread
 * 3. creates shared memory region
 * Returns 0 on success, -1 on error 
 */
int32 dsmThreadInit(int isMaster, char* mIpAddr, int mPort, char* oIpAddr, int oPort,
        unsigned numPagesToAlloc)
{
    int32               retval;
    pthread_t           threadId[DSM_MAX_THREADS] = {0};
    pthread_attr_t      attr;
    int32               port = -1;
    int8                ipAddr[DSM_MAX_IP_ADDR_LEN];

    dsmEnterFunc();

    /* populate the mmap info struct */
    dsmMmapInfo.isMaster = isMaster;
    dsmMmapInfo.mIpAddr  = mIpAddr;
    dsmMmapInfo.mPort    = mPort;
    dsmMmapInfo.oIpAddr  = oIpAddr;
    dsmMmapInfo.oPort    = oPort;
    dsmMmapInfo.numPagesToAlloc = numPagesToAlloc;

    /* open socket for listening to peer request */
    if (dsmMmapInfo.isMaster) {
        port = mPort;
        memcpy(ipAddr, mIpAddr, strlen(mIpAddr)+1); 
    }
    else {
        port = oPort;
        memcpy(ipAddr, oIpAddr, strlen(oIpAddr)+1); 
    }

    /* This socket accepts all client requests throughout the program */
    retval= dsmOpenSocket(ipAddr, port);
    if (-1 == retval) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Error opening socket!\n");
        dsmExitFunc();
        return -1;
    }

    /* open another socket for sending request to peer */
    retval = dsmCreateSocket();
    if (-1 == retval) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Error encountered in creating a socket!\n");
        if (errno == EPROTONOSUPPORT) {
            dsmPrintLog(DSM_TRACE_TYPE_INFO, "errno: [%d], Error Desc: "
                    "[No protocol support]\nAborting...\n", errno);
        }
        else if(errno == ENFILE || errno == EMFILE) {
            dsmPrintLog(DSM_TRACE_TYPE_INFO, "errno: [%d], Error Desc : "
                    "[Too many file descriptors open]\n Aborting...\n", errno);
        }
        else if (errno == EACCES) {
            dsmPrintLog(DSM_TRACE_TYPE_INFO, "errno: [%d], Error Desc: "
                    "[Not enough privileges to create an AF_INET socket]\n"
                    "Aborting...\n", errno);
        }
        abort();
    }

    /* intialize thread with default attributes;
     * make thread detachable and set contention scope to system level */
    pthread_attr_init(&attr); 
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    /* spawn the communication thread */
    retval = pthread_create(&threadId[DSM_COMMUNICATION_THREAD], NULL, dsmAcceptAndRead,
            (void *)&dsmSockInfo.serverSd);
    if (0 != retval) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Comm. Thread creation failed with "
                "errno: %d\n", errno);
        return -1;
    }
    dsmPrintLog(DSM_TRACE_TYPE_INFO, "Comm. Thread created with id: %x\n",
            threadId[DSM_COMMUNICATION_THREAD]);

    /* initialize shared memory region */
    dsmSharedMemoryInit();

    dsmExitFunc();
    return 0;
}

/*
 * Initializes the page table for the shared region
 * Master: Initially has access to all the pages
 */
void dsmInitPageTable()
{
    int32 i = 0;

    dsmEnterFunc();
    for (i = 0; i < dsmMmapInfo.numPagesToAlloc; i += 1) {
        if (dsmMmapInfo.isMaster) {
            dsmPageTable[i].owner = true;
            dsmPageTable[i].pageStatus = DSM_PAGE_PRESENT;
        }
        else {
            dsmPageTable[i].owner = false;
            dsmPageTable[i].pageStatus = DSM_PAGE_NOT_PRESENT;
        }
        pthread_mutex_init(&dsmPageTable[i].pteMutexVar, NULL);
        pthread_cond_init(&dsmPageTable[i].pteCondVar, NULL);
    }
    dsmExitFunc();
}

void initializeDSM(int ismaster, char * masterip, int mport, char *otherip, int oport,
        unsigned numpagestoalloc)
{
    dsmEnterFunc();
    struct sigaction    newAction;
    struct sigaction    oldAction;
    int32 retval = -1;

    /* Register the signal handler 
     * Set up the structure to specify the new action. */
    newAction.sa_sigaction = dsmPageFaultHandler;
    newAction.sa_flags = SA_SIGINFO;
    sigemptyset (&newAction.sa_mask);
    
    sigaction(SIGSEGV, NULL, &oldAction);
    if (oldAction.sa_handler != SIG_IGN) {
        if (-1 == sigaction (SIGSEGV, &newAction, NULL)) {
            dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Signal Registration failed!\n");
            abort();
        }
    }
    dsmPrintLog(DSM_TRACE_TYPE_DEBUG, "Handler for SIGSEGV registered!\n");

    /* initialize the threads */
    retval = dsmThreadInit(ismaster, masterip, mport, otherip, oport, numpagestoalloc);

    if(-1 == retval) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Error in thread initialization! Aborting...\n");
        abort();
    }

    /* initialize page table */
    dsmInitPageTable();

    while (NULL == pDsmSharedRegion) {
        usleep(1000);
    }
    dsmExitFunc();
}

void * getsharedregion()
{
    dsmEnterFunc();
    return pDsmSharedRegion;
    dsmExitFunc();
}

void dsmPrintf(const char *format, ...)
{
    va_list     varList;

    va_start(varList, format);
    vprintf(format, varList);
}


