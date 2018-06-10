#include "dsm_ptab.h"
#include "dsm_util.h"

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

static void freeProcessTableEntry (dsm_proc_node *node) {

    // Recursive guard.
    if (node == NULL) {
        return;
    }

    // Free next, then self.
    freeProcessTableEntry(node->next);
    free(node);
}

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

    // If the file-descriptor doesn't exist, resize the table to fit it.
    while (fd >= ptab->size) {
        ptab->size *= 2;
        ptab->tab = realloc(ptab->tab, ptab->size);
    }

    // Verify process doesn't already exist: If you resize it didn't, but whatever.
    if (getProcessTableEntry(ptab->tab[fd], pid) != NULL) {
        dsm_cpanic("dsm_setProcessTableEntry", "Entry already exists!");
    }

    // Allocate a new node.
    if ((node = malloc(sizeof(dsm_proc_node))) == NULL) {
        dsm_panic("dsm_setProcessTableEntry: Allocation failure!");
    }

    // Configure node as new head of linked-list.
    *node = (dsm_proc_node){
        .next = ptab->tab[fd],
        .proc = (dsm_proc) {
            .gid = ptab->next_gid++,
            .pid = pid,
            .sem_id = -1,
            .flags = 0
        }
    };

    // Increment the process table count.
    ptab->nproc++;

    // Insert node as linked list head and return pointer.
    return &((ptab->tab[fd] = node)->proc);
}

// Returns a process for the given file-descriptor and pid. May return NULL.
dsm_proc *dsm_getProcessTableEntry (dsm_ptab *ptab, int fd, int pid) {

    // Check if fd in range.
    if (fd >= ptab->size || fd < 0) {
        return NULL;
    }

    return getProcessTableEntry(ptab->tab[fd], pid);
}

// Removes a process for the given file-descriptor and pid. 
void dsm_remProcessTableEntry (dsm_ptab *ptab, int fd, int pid) {
    int didExist = 0;

    // Fault on requests out of range.
    if (fd >= ptab->size || fd < 0) {
        dsm_cpanic("dsm_remProcessTableEntry", "fd out of range!");
    }

    // Remove process. Update list head.
    ptab->tab[fd] = remProcessTableEntry(ptab->tab[fd], pid, &didExist);

    // If the process existed, then decrement the count.
    ptab->nproc -= didExist;
}

// Returns next process with matching sem_id. Returns NULL if none found.
dsm_proc *dsm_getProcessTableEntryWithSemID (dsm_ptab *ptab, int sem_id) {

    // For all connections, search all linked lists.
    for (int i = 0; i < ptab->size; i++) {
        for (dsm_proc_node *n = ptab->tab[i]; n != NULL; n = n->next) {
            if (n->proc.sem_id == sem_id) {
                return &(n->proc);
            }
        }
    }
    return NULL;
}

// Frees the process table.
void dsm_freeProcessTable (dsm_ptab *ptab) {
    for (int i = 0; i < ptab->size; i++) {
        freeProcessTableEntry(ptab->tab[i]);
    }
    free(ptab->tab);
    free(ptab);
}