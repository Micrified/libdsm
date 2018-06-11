CC= gcc
CFLAGS=-Wall -Wextra -Werror -Wno-unused-function -pedantic
LIBS=-pthread -lrt
IDIR=./include/
SDIR=./src/
TDIR=./tests/
BIN=./bin/

# FILE LISTS

SERVER_FILES=${SDIR}dsm_server.c ${SDIR}dsm_msg.c ${SDIR}dsm_htab.c ${SDIR}dsm_inet.c ${SDIR}dsm_poll.c ${SDIR}dsm_ptab.c ${SDIR}dsm_sem_htab.c ${SDIR}dsm_stab.c ${SDIR}dsm_util.c ${SDIR}dsm_opqueue.c

TEST_MSG_FILES=${TDIR}dsm_test_msg.c ${SDIR}dsm_msg.c ${SDIR}dsm_util.c

TEST_HTAB_FILES=${TDIR}dsm_test_htab.c ${SDIR}dsm_htab.c ${SDIR}dsm_util.c

TEST_STAB_FILES=${TDIR}dsm_test_stab.c ${SDIR}dsm_stab.c ${SDIR}dsm_util.c

TEST_SEM_FILES=${SDIR}dsm_sem_htab.c ${SDIR}dsm_htab.c ${SDIR}dsm_util.c ${SDIR}dsm_stab.c

TEST_PTAB_FILES=${TDIR}dsm_test_ptab.c ${SDIR}dsm_ptab.c ${SDIR}dsm_util.c

TEST_SERVER_FILES=${TDIR}dsm_test_server.c ${SDIR}dsm_msg.c ${SDIR}dsm_inet.c ${SDIR}dsm_util.c

# PRGM RULES

server: ${SERVER_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -g -o ${BIN}dsm_server ${SERVER_FILES} ${LIBS}


# TEST RULES

tests: test_server

test_msg: ${TEST_MSG_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -g -o ${BIN}dsm_test_msg ${TEST_MSG_FILES} ${LIBS}

test_htab: ${TEST_HTAB_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -g -o ${BIN}dsm_test_htab ${TEST_HTAB_FILES} ${LIBS}

test_stab: ${TEST_STAB_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -g -o ${BIN}dsm_test_stab ${TEST_STAB_FILES} ${LIBS}

test_sem: ${TEST_SEM_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -g -o ${BIN}dsm_test_sem ${TEST_SEM_FILES} ${LIBS}

test_ptab: ${TEST_PTAB_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -g -o ${BIN}dsm_test_ptab ${TEST_PTAB_FILES} ${LIBS}

test_server: ${TEST_SERVER_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -g -o ${BIN}dsm_test_server ${TEST_SERVER_FILES} ${LIBS}



# CLEAN RULES

