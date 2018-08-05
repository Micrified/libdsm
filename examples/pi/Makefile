CC=gcc
CFLAGS= -Wall -Wextra -D_GNU_SOURCE
LIBS=-ldsm -lpthread -lrt -lxed

# BUILD RULES

all: dsm_pi

dsm_pi: dsm_pi.c
	${CC} ${CFLAGS} -o dsm_pi dsm_pi.c ${LIBS}

# CLEAN RULES

clean:
	rm dsm_pi

