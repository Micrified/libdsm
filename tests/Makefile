CC=gcc
CFLAGS= -I ../include -Wall -Wextra -D_GNU_SOURCE
SRC=../src
LIBS= -lpthread -lrt

# BUILD RULES

all: dsm_test_daemon dsm_test_server dsm_test_ptab dsm_test_stab dsm_test_holes

dsm_test_daemon: dsm_test_daemon.c
	@${CC} ${CFLAGS} -o dsm_test_daemon dsm_test_daemon.c ${SRC}/dsm_msg.c ${SRC}/dsm_inet.c ${SRC}/dsm_util.c ${LIBS}

dsm_test_server: dsm_test_server.c
	@${CC} ${CFLAGS} -g -o dsm_test_server dsm_test_server.c ${SRC}/dsm_msg.c ${SRC}/dsm_inet.c ${SRC}/dsm_util.c ${LIBS}

dsm_test_ptab: dsm_test_ptab.c
	@${CC} ${CFLAGS} -o dsm_test_ptab dsm_test_ptab.c ${SRC}/dsm_ptab.c ${SRC}/dsm_util.c ${LIBS}

dsm_test_stab: dsm_test_stab.c
	@${CC} ${CFLAGS} -o dsm_test_stab dsm_test_stab.c ${SRC}/dsm_stab.c ${SRC}/dsm_util.c ${LIBS}

dsm_test_holes: dsm_test_holes.c
	@${CC} ${CFLAGS} -o dsm_test_holes dsm_test_holes.c ${SRC}/dsm_holes.c ${SRC}/dsm_util.c ${LIBS}

# CLEAN RULES

clean:
	@rm dsm_test_daemon
	@rm dsm_test_server
	@rm dsm_test_ptab
	@rm dsm_test_stab
	@rm dsm_test_holes

