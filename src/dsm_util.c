#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "dsm_util.h"


/*
 *******************************************************************************
 *                          I/O Function Definitions                           *
 *******************************************************************************
*/


// Exits fatally with given error message. Also outputs errno.
void dsm_panic (const char *msg) {
	const char *fmt = "[%d] Fatal Error: \"%s\". Errno (%d): \"%s\"\n";
	fprintf(stderr, fmt, getpid(), msg, errno, strerror(errno));
	exit(EXIT_FAILURE);
}

// Exits fatally with given error message. Outputs custom errno.
void dsm_cpanic (const char *msg, const char *reason) {
	const char *fmt = "[%d] Fatal Error: \"%s\". Reason: \"%s\"\n";
	fprintf(stderr, fmt, getpid(), msg, reason);
	exit(EXIT_FAILURE);
}

// Exits fatally with formatted error. Supports tokens: {%s, %d, %f, %u, %p}.
void dsm_panicf (const char *fmt, ...) {
	va_list ap;
	const char *p, *sval;
	int ival;
	unsigned int uval;
	double dval;
	void *pval;

	// Print error start.
	fprintf(stderr, "[%d] Fatal Error: \"", getpid());

	// Initialize argument list.
	va_start(ap, fmt);

	// Parse format string.
	for (p = fmt; *p; p++) {

		// Ignore non-tokens.
		if (*p != '%') {
			putc(*p, stderr);
			continue;
		}

		switch (*(++p)) {
			case 'd': {
				ival = va_arg(ap, int);
				fprintf(stderr, "%d", ival);
				break;
			}
			case 'f': {
				dval = va_arg(ap, double);
				fprintf(stderr, "%f", dval);
				break;
			}
			case 'p': {
				pval = va_arg(ap, void *);
				fprintf(stderr, "%p", pval);
				break;
			}
			case 's': {
				for (sval = va_arg(ap, char *); *sval != '\0'; sval++) {
					putc(*sval, stderr);
				}
				break;
			}
			case 'u': {
				uval = va_arg(ap, unsigned);
				fprintf(stderr, "%u", uval);
			}
		}
	}

	// Print error end.
	fprintf(stderr, "\".\n"); 

	// Clean up.
	va_end(ap);

	// Exit.
	exit(EXIT_FAILURE);
}

// Outputs warning to stderr.
void dsm_warning (const char *msg) {
	const char *fmt = "[%d] Warning: \"%s\".\n";
	fprintf(stderr, fmt, getpid(), msg);
}

// [DEBUG] Redirects output to named file. Returns new fd.
int dsm_setStdout (const char *filename) {
	int fd;

	// Close stdout.
	close(STDOUT_FILENO);

	// Create or open the file: Truncate it.
	if ((fd = open(filename, O_CREAT|O_RDWR|O_TRUNC)) == -1) {
		dsm_panic("Couldn't open/create file!");
	}

	// Dup the new file into stdout's free location.
	return dup(fd);
}

// [DEBUG] Redirects output to xterm window. Returns old stdout. 
int dsm_redirXterm (void) {
	int old_fd = -1, p, fds[2];
	char buf[16];

	// Create/open a pipe.
	if (pipe(fds) == -1) {
		dsm_panic("Couldn't create pipe!");
	}

	// Fork. Check for error.
	if ((p = fork()) == -1) {
		dsm_panic("Couldn't fork!");
	}

	// If child process: Replace STDIN with reading end.
	if (p == 0) {

		// Close writing end of the pipe.
		if(close(fds[1]) == -1) {
			dsm_panic("Couldn't close pipe!");
		}

		// Construct the filepath for xterm.
		sprintf(buf, "/dev/fd/%d", fds[0]);

		// Exec-away.
		execlp("xterm", "xterm", "-e", "cat", buf, NULL);

		// Panic if exec failed.
		dsm_panic("Exec failed!");

	} else {
	
		// Close reading end of the pipe.
		if (close(fds[0]) == -1) {
			dsm_panic("Couldn't close pipe!");
		}

		// Copy stdout.
		if ((old_fd = dup(STDOUT_FILENO)) == -1) {
			dsm_panic("Couldn't dup STDOUT!");
		}

		// Replace STDOUT with writing end.
		if (close(STDOUT_FILENO) == -1 || dup(fds[1]) == -1) {
			dsm_panic("Error replacing STDOUT!");
		}

	}

	return old_fd;
}


/*
 *******************************************************************************
 *                       Semaphore Function Definitions                        *
 *******************************************************************************
*/


// Increments a semaphore. Panics on error.
void dsm_up (sem_t *sp) {
	if (sem_post(sp) == -1) {
		dsm_panic("Couldn't \"up\" semaphore!");
	}
}

// Decrements a semaphore. Panics on error.
void dsm_down (sem_t *sp) {
	if (sem_wait(sp) == -1) {
		dsm_panic("Couldn't \"down\" semaphore!");
	}
}

// Returns a semaphore's value. Panics on error.
int dsm_getSemValue (sem_t *sp) {
	int v = -1;
	
	if (sem_getvalue(sp, &v) == -1) {
		dsm_panic("Couldn't get semaphore value!");
	}

	return v;
}

// Unlinks a named semaphore. Exits fatally on error.
void dsm_unlinkNamedSem (const char *name) {
	if (sem_unlink(name) == -1) {
		dsm_panicf("Couldn't unlink named semaphore: \"%s\"!", name);
	}
}


/*
 *******************************************************************************
 *                         Memory Function Definitions                         *
 *******************************************************************************
*/

// Allocates a zeroed block of memory. Exits fatally on error.
void *dsm_zalloc (size_t size) {
	void *p;

	// Verify allocation success.
	if ((p = malloc(size)) == NULL) {
		dsm_cpanic("dsm_zalloc", "Allocation failed!");
	}

	return memset(p, 0, size);
}

// Sets the given protections to a memory page. Exits fatally on error.
void dsm_mprotect (void *address, size_t size, int flags) {

	// Exit fatally if protection fails.
	if (mprotect(address, size, flags) == -1) {
		dsm_panic("Couldn't protect specified page!");
	}

}

// Allocates a page-aligned slice of memory. Exits fatally on error.
void *dsm_pageAlloc (void *address, size_t size) {
	size_t alignment = sysconf(_SC_PAGE_SIZE);

	if (posix_memalign(&address, alignment, size) == -1) {
		dsm_panic("Couldn't allocate aligned page!");
	}

	return address;
}

// Unlinks a shared memory file. Exits fatally on error.
void dsm_unlinkSharedFile (const char *name) {
	if (shm_unlink(name) == -1) {
		dsm_panicf("Couldn't unlink shared file: \"%s\"!", name);
	}
}


/*
 *******************************************************************************
 *                        Hashing Function Definitions                         *
 *******************************************************************************
*/


// Daniel J. Bernstein's hashing function. General Purpose.
unsigned int DJBHash (const char *s, size_t length) {
	unsigned int hash = 5381;
	unsigned int i = 0;

	for (i = 0; i < length; ++i, ++s) {
		hash = ((hash << 5) + hash) + (*s);
	}

	return hash;
}