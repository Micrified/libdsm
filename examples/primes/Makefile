CC=gcc
CFLAGS= -Wall -Wextra -D_GNU_SOURCE
LIBS=-ldsm -lpthread -lrt -lxed

# BUILD RULES

all: dsm_primes seq_primes

dsm_primes: dsm_primes.c
	${CC} ${CFLAGS} -g -o dsm_primes dsm_primes.c ${LIBS} -lm

seq_primes: seq_primes.c
	${CC} ${CFLAGS} -o seq_primes seq_primes.c -lm



# CLEAN RULES

clean:
	rm dsm_primes
