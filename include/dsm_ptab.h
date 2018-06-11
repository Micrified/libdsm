#if !defined(DSM_PTAB_H)
#define DSM_PTAB_H


/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/


// Default number of expected machines.
#define DSM_PTAB_NFD                32


/*
 *******************************************************************************
 *                              Type Definitions                               *
 *******************************************************************************
*/


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
    dsm_proc proc;					// Process.
    struct dsm_proc_node *next;		// Link to next node.
} dsm_proc_node;


// Type describing a simple process table.
typedef struct dsm_ptab {
    unsigned int next_gid;          // Next available global identifier.
    unsigned int size;              // The number of linked-lists.
    unsigned int nproc;             // Number of logged processes.
	unsigned int nstopped;			// The number of stopped processes.
	unsigned int nblocked;			// The number of blocked processes.
	unsigned int nready;			// The number of ready processes.
    dsm_proc_node **tab;            // Table of linked-lists.
} dsm_ptab;


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// Allocates and initializes a new process table. Exits fatally on error.
dsm_ptab *dsm_initProcessTable (unsigned int size);

// Registers a process for the given file-descriptor (index). Returns pointer.
dsm_proc *dsm_setProcessTableEntry (dsm_ptab *ptab, int fd, int pid);

// Returns a process for the given file-descriptor and pid. May return NULL.
dsm_proc *dsm_getProcessTableEntry (dsm_ptab *ptab, int fd, int pid);

// Removes a process for the given file-descriptor and pid. 
void dsm_remProcessTableEntry (dsm_ptab *ptab, int fd, int pid);

// Frees all processes for the given file-descriptor. Sets slot to NULL.
void dsm_remProcessTableEntries (dsm_ptab *ptab, int fd);

// Returns fd of process with semaphore ID/ -1 if none. Copies to proc_p.
int dsm_getProcessTableEntryWithSemID (dsm_ptab *ptab, int sem_id, 
	dsm_proc *proc_p);

// [DEBUG] Prints the process table.
void dsm_showProcessTable (dsm_ptab *ptab);

// Frees the process table.
void dsm_freeProcessTable (dsm_ptab *ptab);


#endif