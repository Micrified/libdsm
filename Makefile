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

# PRGM RULES


# TEST RULES

tests: test_msg test_htab

test_msg: ${TEST_MSG_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -o ${BIN}dsm_test_msg ${TEST_MSG_FILES} ${LIBS}

test_htab: ${TEST_HTAB_FILES}
	${CC} ${CFLAGS} -I ${IDIR} -o ${BIN}dsm_test_htab ${TEST_HTAB_FILES} ${LIBS}

# CLEAN RULES

