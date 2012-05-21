
DSM_ROOT= ${PWD}
CC= g++
CFLAGS= -I /usr/include -m32 -g3 
#CFLAGS= -I /usr/include -m32 -g3 -D DSM_ENABLE_LOG
SYS_LIBS= -lpthread
SYS_LIB_PATH= /lib/
OBJECTS= dsm_init.o dsm_socket.o dsm_main.o test.o
BIN= test
