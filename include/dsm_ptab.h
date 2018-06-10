#if !defined(DSM_PTAB_H)
#define DSM_PTAB_H

// Default number of expected machines.
#define DSM_PTAB_NFD

// Bit-field enumerating various process mode flags.
typedef struct dsm_pstate {
    unsigned int is_stopped : 1;    // Process stopped (ongoing write).
    unsigned int is_blocked : 1;    // Process blocked (barrier or semaphore)
    unsigned int is_queued  : 1;    // Process queued for operation.
} dsm_pstate;

// Type describing a process.
typedef struct dsm_proc {
    int gid;                        // Global process ID.
    int pid;                        // Local process ID.
    int sem_id;                     // ID of semaphore process is blocked on.
    dsm_pstate flags;               // Process mode flags.
} dsm_proc;

// Node for linked processes.
typedef struct dsm_proc_node {
    dsm_proc proc;
    struct dsm_proc_node *next;
} dsm_proc_node;

// Type describing a simple process table.
typedef struct dsm_ptab {
    unsigned int next_gid;          // Next available global identifier.
    unsigned int size;              // The number of linked-lists.
    unsigned int nproc;             // The number of logged processes.
    dsm_proc_node **tab;            // Table of linked-lists.
} dsm_ptab;

// Allocates and initializes a new process table. Exits fatally on error.
dsm_ptab *dsm_initProcessTable (unsigned int size);

// Registers a process for the given file-descriptor (index). Returns pointer.
dsm_proc *dsm_setProcessTableEntry (dsm_ptab *ptab, int fd, int pid);

// Returns a process for the given file-descriptor and pid. May return NULL.
dsm_proc *dsm_getProcessTableEntry (dsm_ptab *ptab, int fd, int pid);

// Removes a process for the given file-descriptor and pid. 
void dsm_remProcessTableEntry (dsm_ptab *ptab, int fd, int pid);

// Returns next process with matching sem_id. Returns NULL if none found.
dsm_proc *dsm_getProcessTableEntryWithSemID (dsm_ptab *ptab, int sem_id);

// Frees the process table.
void dsm_freeProcessTable (dsm_ptab *ptab);


#endif