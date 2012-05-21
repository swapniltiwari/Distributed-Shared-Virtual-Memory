
include Makefile.inc

# --- targets
all: ${BIN}
${BIN}: $(OBJECTS) 
	$(CC) -m32 -o ${BIN} -L${SYS_LIB_PATH} ${SYS_LIBS} $(OBJECTS)
        
dsm_init.o:
	$(CC) $(CFLAGS) -c ${DSM_ROOT}/dsm_init.c

dsm_main.o:
	$(CC) $(CFLAGS) -c ${DSM_ROOT}/dsm_main.c

dsm_socket.o:
	$(CC) $(CFLAGS) -c ${DSM_ROOT}/dsm_socket.c

test.o:
	$(CC) $(CFLAGS) -c ${DSM_ROOT}/test.c 

# --- remove binary and executable files
clean:
	rm -f ${BIN} $(OBJECTS)

    
