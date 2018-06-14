CC=gcc
CFLAGS= -Wall -Wextra -D_GNU_SOURCE
LIBS=-ldsm -lpthread -lrt -lxed

# BUILD RULES

all: pingpong_busy_wait pingpong_semaphore

pingpong_busy_wait: pingpong_busy_wait.c
	${CC} ${CFLAGS} -o pingpong_busy_wait pingpong_busy_wait.c ${LIBS}

pingpong_semaphore: pingpong_semaphore.c
	${CC} ${CFLAGS} -o pingpong_semaphore pingpong_semaphore.c ${LIBS}

