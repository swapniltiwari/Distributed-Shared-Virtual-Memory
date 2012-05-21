
#include "dsm_types.h"
#include "dsm_defs.h"
#include "dsm_socket.h"
#include "dsm_prototype.h"

/*
 * decodes header info from msg buffer and calls appropriate handler function
 * Returns void
 */
void dsmDecodeMsg(void *buffer)
{
    dsmMsgType      msgType;
    dsmMsg          msg;
    uInt32          payloadLen = 0;
    uInt8*          pPayload = NULL;

    dsmEnterFunc();

    /* decode header info */
    msg = *(dsmMsg*)buffer;
    msgType = msg.msgType;
    payloadLen = msg.payloadLen;
    pPayload = (uInt8*)((uInt8*)buffer + sizeof(dsmMsgType) + sizeof(uInt32));

    /* depending on msg type invoke its handler */
    switch (msgType) {
        case DSM_MSG_INIT_SHARED_REGION_REQ:
            dsmPrintLog(DSM_TRACE_TYPE_INFO, "Message rcvd with API id: "
                    "[DSM_MSG_INIT_SHARED_REGION_REQ]\n");
            dsmInitSharedRegionReqHandler(pPayload);
            break;
        case DSM_MSG_INIT_SHARED_REGION_RSP:
            dsmPrintLog(DSM_TRACE_TYPE_INFO, "Message rcvd with API id: "
                    "[DSM_MSG_INIT_SHARED_REGION_RSP]\n");
            dsmInitSharedRegionRspHandler(pPayload);
            break;
        case DSM_MSG_PAGE_REQ:
            dsmPrintLog(DSM_TRACE_TYPE_INFO, "Message rcvd with API id: "
                    "[DSM_MSG_PAGE_REQ]\n");
            dsmPageReqHandler(pPayload);
            break;
        case DSM_MSG_PAGE_RSP:
            dsmPrintLog(DSM_TRACE_TYPE_INFO, "Message rcvd with API id: "
                    "[DSM_MSG_PAGE_RSP]\n");
            dsmPageRspHandler(pPayload);
            break;
        default:
            dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Invalid Msg type\n");
    }
    dsmExitFunc();
}

/*
 * prepares shared region rsp msg containing shared region base addr
 * and sends back to peer.
 * Returns 0 on success, -1 on failure
 */
int dsmInitSharedRegionReqHandler(void* payload)
{
    dsmMsg*     pMsg = NULL;
    uInt8*      pPayload = NULL;
    uInt8       headerLen = 0;
    uInt32      payloadLen = 0;
    
    dsmEnterFunc();

    /* prepare msg to send to peer */
    headerLen = sizeof(uInt32) + sizeof(uInt32);
    payloadLen = sizeof(uInt32);
    pMsg = (dsmMsg*)malloc(headerLen + payloadLen);
    if (NULL == pMsg) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Memory allocation failed for [%d] "
                "bytes\n", headerLen + payloadLen);
        dsmExitFunc();
        return -1;
    }
    pMsg->msgType = DSM_MSG_INIT_SHARED_REGION_RSP;
    pMsg->payloadLen = sizeof(uInt32);
    memcpy(pMsg->payload, &pDsmSharedRegion, sizeof(uInt32));

    /* send msg and free the memory */
    if (-1 == dsmSendMsg(dsmSockInfo.currentClientSd, pMsg)) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Msg send failed for msg with API Id: "
                "[DSM_MSG_INIT_SHARED_REGION_RSP]\n");
        dsmExitFunc();
        return -1;
    }
    free(pMsg);

    dsmExitFunc();
    return 0;
}

/*
 * assigns shared region base addr rcvd in payload to global shared region variable
 * Returns 0 on success, -1 on failure
 */
int dsmInitSharedRegionRspHandler(void* payload)
{
    dsmEnterFunc();
    pDsmMasterInitAddr = (int*)(*(int*)payload);
    dsmExitFunc();
    return 0;
}

/*
 * make requested page read-only and prepares copy of requested page;
 * sends requested to peer and make page inaccessible on the local machine
 * Returns 0 on success, -1 on failure
 */
int dsmPageReqHandler(void* payload)
{
    dsmMsg*             pMsg = NULL;
    uInt32              pageOffset = 0;
    uInt8*              pageBaseAddr = NULL;
    uInt8               pageBuffer[DSM_PAGE_SIZE] = {0};

    dsmEnterFunc();
    pageOffset = *(int*)payload;

    /* acquire lock on the page */
    pthread_mutex_lock(&dsmPageTable[pageOffset].pteMutexVar);
	dsmPrintLog(DSM_TRACE_TYPE_DEBUG, "Mutex Lock acquired successfully\n");

    /* make the page read only */
    pageBaseAddr = (uInt8*)pDsmSharedRegion + (pageOffset * DSM_PAGE_SIZE);
    dsmPrintLog(DSM_TRACE_TYPE_INFO, "Page Transfer Request from slave with "
            "addr: [%p]\n", pageBaseAddr);
    mprotect(pageBaseAddr, DSM_PAGE_SIZE, PROT_READ);
    
    /* copy the page */
    memcpy(pageBuffer, pageBaseAddr, DSM_PAGE_SIZE);

    /* make page inaccessible and update page table */
    mprotect(pageBaseAddr, DSM_PAGE_SIZE, PROT_NONE);
    dsmPageTable[pageOffset].owner = false;
    dsmPageTable[pageOffset].pageStatus = DSM_PAGE_IN_TRANSFER;

    /* send msg and free the memory; payload = page offset + page
     * make page unavailable on the local machine */
    pMsg = (dsmMsg*)malloc(sizeof(dsmMsg) + sizeof(uInt32) + DSM_PAGE_SIZE);
    if (NULL == pMsg) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Memory allocation failed for [%d] "
                "bytes\n", sizeof(dsmMsg) + sizeof(uInt32) + DSM_PAGE_SIZE);
        dsmExitFunc();
        return -1;
    }
    pMsg->msgType = DSM_MSG_PAGE_RSP;
    pMsg->payloadLen = DSM_PAGE_SIZE + sizeof(uInt32);
    memcpy(pMsg->payload, &pageOffset, sizeof(uInt32));
    memcpy((void*)(pMsg->payload+sizeof(uInt32)), pageBuffer, DSM_PAGE_SIZE);

    if (-1 == dsmSendMsg(dsmSockInfo.currentClientSd, pMsg)) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Msg send failed for msg with API Id: "
                "[DSM_MSG_PAGE_RSP]\n");
        free(pMsg);
        dsmExitFunc();
        return -1;
    }
    dsmPageTable[pageOffset].pageStatus = DSM_PAGE_NOT_PRESENT;

    /* Signal the other waiting thread if any */
    pthread_cond_signal(&dsmPageTable[pageOffset].pteCondVar);
    pthread_mutex_unlock(&dsmPageTable[pageOffset].pteMutexVar);
	dsmPrintLog(DSM_TRACE_TYPE_DEBUG, "Mutex Lock released successfully\n");

    /* see can cause seg fault
    dsmPrintLog(DSM_TRACE_TYPE_INFO, "Page with base addr [%p] transfered\n",
            pageBaseAddr);
    */
    free(pMsg);
    dsmExitFunc();
    return 0;
}

/*
 * copies the page rcvd to the corresponding shared memory region
 * and assigns read-write permission to it.
 * Returns 0 on success, -1 on failure
 */
int dsmPageRspHandler(void* payload)
{
    uInt32              pageOffset = 0;
    uInt8*              pageBaseAddr = NULL;

    dsmEnterFunc();
    /* make the page write only */
    pageOffset = *(int*)payload;
    pageBaseAddr = (uInt8*)pDsmSharedRegion + (pageOffset * DSM_PAGE_SIZE);
    mprotect(pageBaseAddr, DSM_PAGE_SIZE, PROT_WRITE);
    dsmPrintLog(DSM_TRACE_TYPE_INFO, "New page with base addr [%p] rcvd from "
            "owner\n", pageBaseAddr);

    /* copy the page */
    memcpy(pageBaseAddr, ((uInt8*)payload)+sizeof(uInt32), DSM_PAGE_SIZE);

    /* make the page accessible and update page table */
    mprotect(pageBaseAddr, DSM_PAGE_SIZE, PROT_WRITE | PROT_READ);
    dsmPageTable[pageOffset].owner = true;
    dsmPageTable[pageOffset].pageStatus = DSM_PAGE_PRESENT;

    dsmPrintLog(DSM_TRACE_TYPE_INFO, "New page with base addr [%p] updated "
            "locally\n", pageBaseAddr);
    dsmExitFunc();
    return 0;
}

/*
 * signal handler invoked on page fault
 * prepares page request msg and sends request to peer.
 * waits for response from peer
 * Returns void
 */
void dsmPageFaultHandler(int signal, siginfo_t *data, void *other)
{
	int         offsetPageMultiple = -1;
	int         retval = -1;

	dsmPrintLog(DSM_TRACE_TYPE_INFO, "Page Fault occured for address [%p] "
            "with code [%d]\n", data->si_addr, data->si_code);

        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Access to invalid region: [%p]\n", data->si_addr);
	/*Calculate the page offset in multiples of page size
	 * offsetPageMultiple is an index into the Page Table array */
	offsetPageMultiple = ((char*)data->si_addr - (char*)pDsmSharedRegion)/DSM_PAGE_SIZE;
	dsmPrintLog(DSM_TRACE_TYPE_DEBUG, "Page offset: [%d]\n", offsetPageMultiple);

	pthread_mutex_lock(&(dsmPageTable[offsetPageMultiple].pteMutexVar));
	dsmPrintLog(DSM_TRACE_TYPE_DEBUG, "Mutex Lock acquired successfully\n");

	/* The page fault handler should continue only if the page is not present
     * at the machine */
	if (dsmPageTable[offsetPageMultiple].pageStatus != DSM_PAGE_NOT_PRESENT) {
		dsmPrintLog(DSM_TRACE_TYPE_DEBUG,"Waiting for signal...\n");
		while (!pthread_cond_wait(&dsmPageTable[offsetPageMultiple].pteCondVar,
                    &dsmPageTable[offsetPageMultiple].pteMutexVar))
			dsmPrintLog(DSM_TRACE_TYPE_ERROR, "pthread_cond_wait returned erroneously\n");
	}

	if (dsmPageTable[offsetPageMultiple].pageStatus != DSM_PAGE_IN_TRANSFER) {
		/* request for page which caused the segmentation fault */
		dsmCreateSocket();
		do {
			usleep(100);
            if (dsmMmapInfo.isMaster) {
			    retval = dsmConnectToPeer(dsmSockInfo.reqSockSd, dsmMmapInfo.oIpAddr,
					dsmMmapInfo.oPort);
            }
            else {
			    retval = dsmConnectToPeer(dsmSockInfo.reqSockSd, dsmMmapInfo.mIpAddr,
					dsmMmapInfo.mPort);
            }
		} while (-1 == retval);

		/* Compose the request message */
		dsmMsg msg;
		memset(&msg, 0, sizeof(dsmMsg));
		msg.msgType = DSM_MSG_PAGE_REQ;
		msg.payloadLen = sizeof(uInt32);
		memcpy(msg.payload, &offsetPageMultiple, msg.payloadLen);

		/* Request the page from the other process; 
         * Set the Page table entry accordingly;
         * Block the handler to receive the page */
		dsmPageTable[offsetPageMultiple].pageStatus = DSM_PAGE_REQUESTED;
		dsmSendMsg(dsmSockInfo.reqSockSd, &msg);
	    dsmPrintLog(DSM_TRACE_TYPE_INFO, "Page request sent for page with "
                "offset [%d]\n", offsetPageMultiple);
		dsmPageTable[offsetPageMultiple].pageStatus = DSM_PAGE_IN_TRANSFER;
		dsmRecvMsg(dsmSockInfo.reqSockSd);
	    dsmPrintLog(DSM_TRACE_TYPE_INFO, "Response rcvd for page with "
                "offset [%d]\n", offsetPageMultiple);

		close(dsmSockInfo.reqSockSd);

		/* Release the mutex variable */
		pthread_mutex_unlock(&dsmPageTable[offsetPageMultiple].pteMutexVar);
	    dsmPrintLog(DSM_TRACE_TYPE_DEBUG, "Mutex Lock released successfully\n");
        dsmExitFunc();
	}
}


