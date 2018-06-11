#if !defined(DSM_SEM_HTAB_H)
#define DSM_SEM_HTAB_H

#include "dsm_htab.h"

/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/


// Number of buckets in the semaphore hash-table.
#define DSM_SEM_HTAB_LENGTH   64


/*
 *******************************************************************************
 *                              Type Definitions                               *
 *******************************************************************************
*/


// Type describing a named semaphore.
typedef struct dsm_sem_t {
    int sem_id;                         // Semaphore identifier.
    unsigned int value;                 // Semaphore value.
} dsm_sem_t;


// Custom redefinition of dsm_htab.
typedef dsm_htab dsm_sem_htab;


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// Initializes semaphore hash table. Returns pointer. Exits fatally on error.
dsm_sem_htab *dsm_initSemHashTable (void);

// Registers new semaphore in table. Returns pointer. Exits fatally on error.
dsm_sem_t *dsm_setSemHashTableEntry (dsm_sem_htab *htab, char *sem_name);


#endif