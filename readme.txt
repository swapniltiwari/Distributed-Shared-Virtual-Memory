

1. Master starts and maps a region of memory as shared region. When client starts, it request for shared region base address from master and maps the shared region on the same base address. 

2. There is no dependency in the order in which the application should be started. If the client is started first, it waits for master to come up and then requests shared region base address from master node. 

3. Debug Information: To allow the application to print debug information, enable DSM_ENABLE_LOG flag in Makefile.inc

