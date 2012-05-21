
#include "dsm_types.h"
#include "dsm_socket.h"
#include "dsm_defs.h"
#include "dsm_prototype.h"

/* global definitions */
dsmSocketInfo   dsmSockInfo;


/*
 * Creates a tcp socket; sets global socket fd
 * Returns 0 on success, -1 on failure
 */
int32 dsmCreateSocket()
{
    int32               socketDesc = -1;
    const int32         optVal = 1;
    const socklen_t     optLen = sizeof(optVal);
    int32               retVal = -1;

    dsmEnterFunc();

    /* create a tcp socket; make the addr reusable */
    socketDesc = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == socketDesc) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "socket call failed with errno: [%d]\n",
                errno);
        dsmExitFunc();
        return -1;
    }
    retVal = setsockopt(socketDesc, SOL_SOCKET, SO_REUSEADDR, (void*) &optVal, optLen); 
    if (-1 == retVal) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "setsockopt failed with errno: [%d]\n",
                errno);
        dsmExitFunc();
        return -1;
    }

    /* set the client socket descriptor */
    dsmSockInfo.reqSockSd = socketDesc;
    dsmPrintLog(DSM_TRACE_TYPE_DEBUG, "Socket fd [%d] opened for sending req to "
            "peer\n", socketDesc);
    dsmExitFunc();
    return 0;
}

/*
 * Open a tcp sockets and listen on it for request;
 * Returns 0 on success, -1 on failure
 */
int32 dsmOpenSocket(int8 *ipAddr, int32 port)
{
	struct sockaddr_in  serverAddr, cliAddr;
    struct in_addr      ip_addr;
	int32               socketDesc = -1;
    const int32         optVal = 1;
    const socklen_t     optLen = sizeof(optVal);
    int32               retVal = -1;
    
    dsmEnterFunc();

    /* create a tcp socket; make the addr reusable */
    socketDesc = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == socketDesc) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "socket call failed with errno: [%d]\n",
                errno);
        dsmExitFunc();
        return -1;
    }
    retVal = setsockopt(socketDesc, SOL_SOCKET, SO_REUSEADDR, (void*) &optVal, optLen); 
    if(-1 == retVal) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "setsockopt failed with errno: [%d]\n",
                errno);
        dsmExitFunc();
        return -1;
    }

    /* Bind to a port and addr */
    inet_aton(ipAddr, &ip_addr);
	serverAddr.sin_family = AF_INET;        
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr = ip_addr.s_addr; 
	bzero(&(serverAddr.sin_zero),8);
	retVal = bind(socketDesc,(struct sockaddr *)&serverAddr,
            sizeof(struct sockaddr));
    if (-1 == retVal) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "bind failed with errno: [%d], "
                "Ip Addr: [%s], Port: [%d]\n", errno, ipAddr, port);
        dsmExitFunc();
        return -1;
    }

    /* listen on the socket */
    if(listen(socketDesc, 10) == -1) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "listen failed with errno: [%d]\n", errno);
        dsmExitFunc();
        return -1;
    }

    /* set the server socket descriptor */
    dsmSockInfo.serverSd = socketDesc;
    dsmPrintLog(DSM_TRACE_TYPE_DEBUG, "Socket fd [%d] opened for listening to req "
            "from peer\n", socketDesc);
    dsmExitFunc();
    return 0;
}

/*
 * Waits for connection from peer; on receving a msg calls decodeMsg
 * Returns 0 on success, -1 on failure
 */
void* dsmAcceptAndRead(void* socketDesc)
{
	struct sockaddr_in      cliAddr;
    int32                   sd = -1;
	socklen_t               size = sizeof(struct sockaddr_in);
    void*                   pReadData = NULL;
    void*                   pBuf = NULL;

    dsmEnterFunc();

    sd = *(int32*)socketDesc;

    /* allocate memory for msg */
    pReadData = malloc(DSM_MAX_MSG_LEN);
    if (NULL == pReadData) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Memory allocation failed for [%d] "
                "bytes\n", DSM_MAX_MSG_LEN);
        return (void*)(-1);
    }
    pBuf = malloc(DSM_MAX_MSG_LEN);
    if (NULL == pBuf) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Memory allocation failed for [%d] "
                "bytes\n", DSM_MAX_MSG_LEN);
        return (void*)(-1);
    }

	while (1)
	{
        /* wait for a connect req */
        memset(&cliAddr, 0, sizeof(struct sockaddr_in));
		int32 clientSd = accept(sd, (struct sockaddr *)&cliAddr, &size);
        if (-1 == clientSd) {
            continue;
        }
        dsmPrintLog(DSM_TRACE_TYPE_INFO, "Connection rcvd from client. "
                "New client fd: [%d]\n", clientSd);
        dsmSockInfo.currentClientSd = clientSd;

		/* read the msg header to get payload length */
		int32 bytesRead = 0, offset = 0;
		int32 bytesToRead = DSM_MSG_HDR_LEN;
		do {
			memset(pBuf, 0, DSM_MAX_MSG_LEN);
			bytesRead = recv(dsmSockInfo.currentClientSd, pBuf, bytesToRead, 0);
            if (-1 == bytesRead) {
				dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Recv from socket fd [%d] failed "
	                "with errno: [%d]\n", socketDesc, errno);
                dsmExitFunc();
				return (void*)-1;
			}
			memcpy(((int8*)pReadData + offset), pBuf, bytesRead);
			offset += bytesRead;
		} while (offset < bytesToRead);
		
        /* read the number of bytes specified by payload length */
		bytesRead = 0;
		uInt32 payloadLen= *(uInt32*)((int32*)pReadData + 1);
		if (payloadLen > 0) {
			while ((offset - DSM_MSG_HDR_LEN) < payloadLen){
			    memset(pBuf, 0, DSM_MAX_MSG_LEN);
				bytesRead = recv(dsmSockInfo.currentClientSd, pBuf, payloadLen, 0);
                if (-1 == bytesRead) {
					dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Recv from socket fd [%d] failed "
						"with errno: [%d]\n", socketDesc, errno);
                    dsmExitFunc();
					return (void*)-1;
				}
				memcpy(((int8*)pReadData + offset), pBuf, bytesRead);
				offset += bytesRead;
			}
		}
	    dsmPrintLog(DSM_TRACE_TYPE_INFO, "Total [%d] bytes rcvd from client fd: [%d]\n",
	        offset, dsmSockInfo.currentClientSd);

        /* decode msg; close connection */
        dsmDecodeMsg(pReadData);
		close(clientSd);		
        dsmSockInfo.currentClientSd = -1;
        usleep(10000);    
    }

    /* free the allocated memory */
    free(pReadData);
    free(pBuf);
    dsmExitFunc();
}

/*
 * Connects to peer specified by addr and port as args
 * Returns 0 on success, -1 on failure
 */
int32 dsmConnectToPeer(int32 socketDesc, int8* ipAddr, int32 port)
{
    struct hostent *master = gethostbyname(ipAddr);   
    struct sockaddr_in      serverAddr;
    int32                   retval = -1;

    serverAddr.sin_family = AF_INET; 
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr = *((struct in_addr *)master->h_addr);
    bzero(&(serverAddr.sin_zero), 8);

    retval = connect(socketDesc, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr_in));
    if (retval != -1) {
        dsmPrintLog(DSM_TRACE_TYPE_INFO, "Connect to [%s] at port [%d] success\n",
                ipAddr, port);
    }

    return retval;
}

/*
 * sends msg on socket, both specified as args 
 * Returns 0 on success, -1 on failure
 */
int32 dsmSendMsg(int32 socketDesc, dsmMsg* pMsg)
{
    uInt8*      pMsgBuf = NULL;
    uInt8*      pBuffer = NULL;
    uInt8       headerLen = 0;
    uInt32      payloadLen = 0;

    dsmEnterFunc();

    /* create msg buffer and send to peer */
    headerLen = (sizeof(pMsg->msgType) + sizeof(pMsg->payloadLen)); 
    memcpy(&payloadLen, &(pMsg->payloadLen), sizeof(uInt32));
    pMsgBuf = (uInt8*)malloc(headerLen + payloadLen);
    if (NULL == pMsgBuf) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Memory allocation failed for [%d] "
                "bytes\n", headerLen + payloadLen);
        return -1;
    }
    memset(pMsgBuf, 0, headerLen + payloadLen);
    pBuffer = pMsgBuf;
    memcpy(pBuffer, pMsg, headerLen);
    pBuffer += headerLen;
    memcpy(pBuffer, pMsg->payload, payloadLen);

    if (-1 == send(socketDesc, pMsgBuf, (headerLen + payloadLen), 0)) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Send to socket fd [%d] failed "
                "with errno: [%d]\n", socketDesc, errno);
        free(pMsgBuf);
        dsmExitFunc();
        return -1;
    }
    
    free(pMsgBuf);
    dsmExitFunc();
    return 0;
}

/*
 * waits for msg on socket specified as arg; on receving msg calls decodeMsg
 * Returns 0 on success, -1 on failure
 */
int32 dsmRecvMsg(int32 socketDesc)
{
    int32 		noOfBytesPeek = 0;
    int32 		bytesRead = 0;
    int32	    offset = 0;
    void*       pReadData = NULL;
    void*       pBuf = NULL;

    dsmEnterFunc();

    /* allocate memory for msg */
    pReadData = malloc(DSM_MAX_MSG_LEN);
    if (NULL == pReadData) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Memory allocation failed for [%d] "
                "bytes\n", DSM_MAX_MSG_LEN);
        dsmExitFunc();
        return (-1);
    }
    pBuf = malloc(DSM_MAX_MSG_LEN);
    if (NULL == pBuf) {
        dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Memory allocation failed for [%d] "
                "bytes\n", DSM_MAX_MSG_LEN);
        dsmExitFunc();
        return (-1);
    }

	/* read the msg header to get payload length */
	int32 bytesToRead = DSM_MSG_HDR_LEN;
	do {
		memset(pBuf, 0, DSM_MAX_MSG_LEN);
		bytesRead = recv(socketDesc, pBuf, bytesToRead, 0);
		if (-1 == bytesRead) {
			dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Recv from socket fd [%d] failed "
                "with errno: [%d]\n", socketDesc, errno);
            dsmExitFunc();
			return -1;
		}
        memcpy(((int8*)pReadData + offset), pBuf, bytesRead);
		offset += bytesRead;

	} while (offset < bytesToRead);
	
    /* read the number of bytes specified by payload length */
	bytesRead = 0;
	uInt32 payloadLen= *(uInt32*)((int32*)pReadData + 1);
	if (payloadLen > 0) {
		while ((offset - DSM_MSG_HDR_LEN) < payloadLen){
		    memset(pBuf, 0, DSM_MAX_MSG_LEN);
			bytesRead = recv(socketDesc, pBuf, payloadLen, 0);
            if (-1 == bytesRead) {
				dsmPrintLog(DSM_TRACE_TYPE_ERROR, "Recv from socket fd [%d] failed "
					"with errno: [%d]\n", socketDesc, errno);
                dsmExitFunc();
				return -1;
			}
			memcpy(((int8*)pReadData + offset), pBuf, bytesRead);
			offset += bytesRead;
		}
	}
    dsmPrintLog(DSM_TRACE_TYPE_INFO, "Total [%d] bytes rcvd from client fd: [%d]\n",
        offset, socketDesc);

    /* decode msg and free memory */
    dsmDecodeMsg(pReadData);
    free(pBuf);
    free(pReadData);

    dsmExitFunc();
    return 0;
}

