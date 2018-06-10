#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dsm_ptab.h"
#include "dsm_util.h"


int main (void) {
    dsm_proc proc, *p;
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

    // Verify the size is okay.
    assert(ptab->nproc == 12);

    // Set some semaphore identifiers on processes 0, and 13.
    assert((p = dsm_getProcessTableEntry(ptab, 0, 0)) != NULL);
    p->sem_id = 42;
    assert((p = dsm_getProcessTableEntry(ptab, 4, 13)) != NULL);
    p->sem_id = 42;

    // Unset the semaphore for all processes that have it.
    while ((fd = dsm_getProcessTableEntryWithSemID(ptab, 42, &proc)) != -1) {
        int pid = proc.pid;
        dsm_remProcessTableEntry(ptab, fd, pid);
        assert(dsm_getProcessTableEntry(ptab, fd, pid) == NULL);
    }

    // Free the process table.
    dsm_freeProcessTable(ptab);

    return 0;
}