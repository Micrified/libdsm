#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dsm_ptab.h"
#include "dsm_util.h"


/*
 *******************************************************************************
 *                        Private Function Definitions                         *
 *******************************************************************************
*/


// Reallocates the array of linked-list pointers.
static void resizeProcessTable (dsm_ptab *ptab, unsigned int size) {

    // Verify input.
    if (ptab == NULL || size < ptab->size) {
        dsm_cpanic("resizeProcessTable", "Invalid argument!");
    }

    // Allocate a new zeroed linked-list table.
    dsm_proc_node **tab = dsm_zalloc(size * sizeof(dsm_proc_node *));

    // Copy over the old entries.
    memcpy(tab, ptab->tab, ptab->size * sizeof(dsm_proc_node *));

    // Free the old table.
    free(ptab->tab);

    // Update with the new table, and set the new size.
    ptab->tab = tab;
    ptab->size = size;
}

// Searches the linked-list for given pid. Returns NULL if not found.
static dsm_proc *getProcessTableEntry (dsm_proc_node *node, int pid) {
    
    // Recursive guard.
    if (node == NULL) {
        return NULL;
    }

    // If current entry, return pointer to ptab.
    if (node->proc.pid == pid) {
        return &(node->proc);
    }

    // Search next node.
    return getProcessTableEntry(node->next, pid);
}

// Removes entry with specified pid from linked-list. Returns self.
static dsm_proc_node *remProcessTableEntry (dsm_proc_node *node, int pid, 
    int *didExist) {
    dsm_proc_node *next;

    // Recursive guard.
    if (node == NULL) {
        return NULL;
    }

    // If current node, remove and return pointer to next.
    if (node->proc.pid == pid) {
        next = node->next;
        *didExist = 1;
        free(node);
        return next;
    }

    // Set next to recursive call, and return pointer.
    node->next = remProcessTableEntry(node->next, pid, didExist);
    return node;
}

// Recursively frees all nodes in the linked list.
static void freeProcessTableEntry (dsm_proc_node *node) {

    // Recursive guard.
    if (node == NULL) {
        return;
    }

    // Free next, then self.
    freeProcessTableEntry(node->next);
    free(node);
}


/*
 *******************************************************************************
 *                         Public Function Definitions                         *
 *******************************************************************************
*/


// Allocates and initializes a new process table. Exits fatally on error.
dsm_ptab *dsm_initProcessTable (unsigned int size) {
    dsm_ptab *ptab;

    // Allocate table.
    if ((ptab = malloc(sizeof(dsm_ptab))) == NULL) {
        dsm_panic("dsm_initProcessTable: Allocation failure!");
    }

    // Configure table, allocate linked-list array.
    *ptab = (dsm_ptab) {
        .next_gid = 0,
        .size = size,
        .nproc = 0,
        .tab = dsm_zalloc(size * sizeof(dsm_proc_node *))
    };

    return ptab;
}

// Registers a process for the given file-descriptor (index). Returns pointer.
dsm_proc *dsm_setProcessTableEntry (dsm_ptab *ptab, int fd, int pid) {
    dsm_proc_node *node;

    // Verify input.
    if (ptab == NULL || fd < 0) {
        dsm_cpanic("dsm_setProcessTableEntry", "Invalid arguments!");
    }

    // Resize the table if the file-descriptor does not have a slot.
    if ((unsigned int)fd >= ptab->size) {
        resizeProcessTable(ptab, MAX(ptab->size * 2, (unsigned int)fd));
    }

    // Verify process doesn't exist: If you resize then it didn't, but eh.
    if (getProcessTableEntry(ptab->tab[fd], pid) != NULL) {
        dsm_cpanic("dsm_setProcessTableEntry", "Entry already exists!");
    }

    // Allocate a new node.
    if ((node = malloc(sizeof(dsm_proc_node))) == NULL) {
        dsm_panic("dsm_setProcessTableEntry: Allocation failure!");
    }

    // Configure node as new head of linked-list.
    *node = (dsm_proc_node) {
        .next = ptab->tab[fd],
        .proc = (dsm_proc) {
            .gid = ptab->next_gid++,
            .pid = pid,
            .sem_id = -1,
            .flags = {0}
        }
    };

    // Increment the process table count.
    ptab->nproc++;

    // Insert node as linked list head and return pointer.
    return &((ptab->tab[fd] = node)->proc);
}

// Returns a process for the given file-descriptor and pid. May return NULL.
dsm_proc *dsm_getProcessTableEntry (dsm_ptab *ptab, int fd, int pid) {

    // Verify input.
    if (ptab == NULL || fd < 0) {
        dsm_cpanic("dsm_getProcessTableEntry", "Invalid arguments!");
    }

    // Check if fd in range.
    if ((unsigned int)fd >= ptab->size) {
        return NULL;
    }

    return getProcessTableEntry(ptab->tab[fd], pid);
}

// Removes a process for the given file-descriptor and pid. 
void dsm_remProcessTableEntry (dsm_ptab *ptab, int fd, int pid) {
    int didExist = 0;

    // Verify arguments.
    if (ptab == NULL || fd < 0 || (unsigned int)fd >= ptab->size) {
        dsm_cpanic("dsm_remProcessTableEntry", "Invalid arguments!");
    }

    // If the entry already exists, error out.
    if (dsm_getProcessTableEntry(ptab, fd, pid) == NULL) {
        dsm_cpanic("dsm_remProcessTableEntry", "No entry exists to remove!");
    }

    // Remove process. Update list head.
    ptab->tab[fd] = remProcessTableEntry(ptab->tab[fd], pid, &didExist);

    // If the process existed, then decrement the count.
    ptab->nproc -= didExist;
}

// Returns fd of process with semaphore ID/ -1 if none. Copies to proc_p.
int dsm_getProcessTableEntryWithSemID (dsm_ptab *ptab, int sem_id, 
	dsm_proc *proc_p) {

    // For all connections, search all linked lists.
    for (unsigned int i = 0; i < ptab->size; i++) {
        for (dsm_proc_node *n = ptab->tab[i]; n != NULL; n = n->next) {
            if (n->proc.sem_id == sem_id) {
                *proc_p = n->proc;
                return i;
            }
        }
    }
    return -1;
}

// [DEBUG] Prints the process table.
void dsm_showProcessTable (dsm_ptab *ptab) {
    printf("nproc = %u\n", ptab->nproc);
    dsm_proc_node *n;

    // Show individual tables.
    for (unsigned int i = 0; i < ptab->size; i++) {
        printf("--------------------------------[fd = %d]------"\
			"--------------------------------\n", i);
        printf(" gid\tpid\tsem_id\ts\tb\tq\n");
        for (n = ptab->tab[i]; n != NULL; n = n->next) {
            char s = (n->proc.flags.is_stopped == 1 ? 'Y' : 'N');
            char b = (n->proc.flags.is_blocked == 1 ? 'Y' : 'N');
            char q = (n->proc.flags.is_queued == 1 ? 'Y' : 'N');
            printf(" %d\t%d\t%d\t%c\t%c\t%c\n", n->proc.gid, 
				n->proc.pid, n->proc.sem_id, s, b, q);
        }
        printf("\n");
    }
}

// Frees the process table.
void dsm_freeProcessTable (dsm_ptab *ptab) {

    for (unsigned int i = 0; i < ptab->size; i++) {
        freeProcessTableEntry(ptab->tab[i]);
    }

    free(ptab->tab);
    free(ptab);
}
