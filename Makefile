CC= gcc
CFLAGS=-Wall -Wextra -Werror -Wno-unused-function -pedantic -D_GNU_SOURCE
LIBS=-lpthread -lrt
IDIR=./include/
SDIR=./src/
ODIR=./obj/
BIN=./bin/

# FILE LISTS

SERVER_FILES=${SDIR}dsm_server.c ${SDIR}dsm_msg.c ${SDIR}dsm_htab.c ${SDIR}dsm_inet.c ${SDIR}dsm_poll.c ${SDIR}dsm_ptab.c ${SDIR}dsm_sem_htab.c ${SDIR}dsm_stab.c ${SDIR}dsm_util.c ${SDIR}dsm_opqueue.c

ARBITER_FILES=${SDIR}dsm_arbiter.c ${SDIR}dsm_msg.c ${SDIR}dsm_inet.c ${SDIR}dsm_poll.c ${SDIR}dsm_ptab.c ${SDIR}dsm_util.c

DSM_FILES=${SDIR}dsm.c ${SDIR}dsm_sync.c ${SDIR}dsm_signal.c


# BUILD RULES

all: setup dsm libdsm.a server

setup:
	mkdir -p bin && mkdir -p obj

libdsm.a: $(patsubst $(SDIR)/%.c,$(ODIR)/%.o,$(SDIR))
	ar -cvq libdsm.a ${ODIR}/*.o

dsm: ${DSM_FILES} ${ARBITER_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -c ${DSM_FILES} ${ARBITER_FILES}
	mv *.o ${ODIR}

server: ${SERVER_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -g -o ${BIN}dsm_server ${SERVER_FILES} ${LIBS}

debug_arbiter: ${ARBITER_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -g -o ${BIN}dsm_arbiter ${ARBITER_FILES} ${LIBS}

debug_dsm: ${ARBITER_FILES} ${DSM_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -g ${DSM_FILES} ${ARBITER_FILES} ${LIBS} -lxed


# INSTALL RULES

install:
	sudo cp libdsm.a /usr/local/lib/
	sudo mkdir -p /usr/local/include/dsm
	sudo cp include/* /usr/local/include/dsm


# UNINSTALL RULES

uninstall:
	sudo rm /usr/local/lib/libdsm.a
	sudo rm -rf /usr/local/include/dsm


# CLEAN RULES

clean:
	test -f /dev/shm/dsm_file && rm /dev/shm/dsm_file
	test -f /dev/shm/sem.dsm_start && rm /dev/shm/sem.dsm_start
	test -f ./libdsm.a && rm ./libdsm.a
	rm ${ODIR}*.o

