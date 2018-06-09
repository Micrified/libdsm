CC= gcc
CFLAGS=-Wall -Wextra -Werror -pedantic
LIBS=-pthread -lrt
IDIR=./include/
SDIR=./src/
TDIR=./tests/
BIN=./bin/

# FILE LISTS

TEST_MSG_FILES=${TDIR}dsm_test_msg.c ${SDIR}dsm_msg.c ${SDIR}dsm_util.c

TEST_HTAB_FILES=${TDIR}dsm_test_htab.c ${SDIR}dsm_htab.c ${SDIR}dsm_util.c

TEST_STAB_FILES=${TDIR}dsm_test_stab.c ${SDIR}dsm_stab.c ${SDIR}dsm_util.c

# PRGM RULES


# TEST RULES

tests: test_stab

test_msg: ${TEST_MSG_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -g -o ${BIN}dsm_test_msg ${TEST_MSG_FILES} ${LIBS}

test_htab: ${TEST_HTAB_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -g -o ${BIN}dsm_test_htab ${TEST_HTAB_FILES} ${LIBS}

test_stab: ${TEST_STAB_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -g -o ${BIN}dsm_test_stab ${TEST_STAB_FILES} ${LIBS}

# CLEAN RULES

