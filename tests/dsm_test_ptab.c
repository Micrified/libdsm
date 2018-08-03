#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dsm_ptab.h"
#include "dsm_util.h"

/* Test Description:
 * This program puts the process table through a couple checks, ensuring that 
 * processes logged with it are successfully retrieved and deleted. The sem-
 * aphore feature is also tested.
*/


int main (void) {
    dsm_proc *p;
    int fd;

    // Initialize process table.
    dsm_ptab *ptab = dsm_initProcessTable(3);
    assert(ptab != NULL);

    // Log some processes.
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 3; j++) {
            dsm_setProcessTableEntry(ptab, i, 3 * i + j);
        }
    }

    // Check PID 3, 8, 14. Then check the table.
    assert((p = dsm_getProcessTableEntry(ptab, 1, 3)) != NULL && p->pid == 3);
    assert((p = dsm_getProcessTableEntry(ptab, 2, 8)) != NULL && p->pid == 8);
    assert((p = dsm_getProcessTableEntry(ptab, 4, 14)) != NULL && p->pid == 14);

    // Remove PID 1, 5, 9.
    dsm_remProcessTableEntry(ptab, 0, 1); assert(dsm_getProcessTableEntry(ptab, 0, 1) == NULL);
    dsm_remProcessTableEntry(ptab, 1, 5); assert(dsm_getProcessTableEntry(ptab, 1, 5) == NULL);
    dsm_remProcessTableEntry(ptab, 3, 9); assert(dsm_getProcessTableEntry(ptab, 3, 9) == NULL);
    assert(ptab->nproc == 12);

    // Set some semaphore identifiers on processes 0, and 13.
    assert((p = dsm_getProcessTableEntry(ptab, 0, 0)) != NULL);
    p->sem_id = 42;
    assert((p = dsm_getProcessTableEntry(ptab, 4, 13)) != NULL);
    p->sem_id = 42;

    // Unset the semaphore for all processes that have it.
    while ((p = dsm_getProcessTableEntryWithSemID(ptab, 42, &fd)) != NULL) {
		int pid = p->pid;
		assert((pid == 0 && fd == 0) || (pid == 13 && fd == 4));
        dsm_remProcessTableEntry(ptab, fd, pid);
        assert(dsm_getProcessTableEntry(ptab, fd, pid) == NULL);
    }

    // Remove all processes under fd 3.
    dsm_remProcessTableEntries(ptab, 3);

    // Free the process table.
    dsm_freeProcessTable(ptab);

	// Print ending message.
	printf("Ok!\n");

    return 0;
}