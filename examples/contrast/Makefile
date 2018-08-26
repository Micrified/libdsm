CC=gcc
CFLAGS= -Wall -Wextra -D_GNU_SOURCE
LIBS=-ldsm -lpthread -lrt -lxed

# BUILD RULES

all: dsm_contrast

dsm_contrast: dsm_contrast.c image.c
	@${CC} ${CFLAGS} -o dsm_contrast dsm_contrast.c image.c ${LIBS} -lm


# CLEAN RULES

clean:
	@rm dsm_contrast

