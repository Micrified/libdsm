#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsm_sem_htab.h"
#include "dsm_htab.h"
#include "dsm_util.h"
#include "dsm_stab.h"


/*
 *******************************************************************************
 *                              Global Variables                               *
 *******************************************************************************
*/


// [EXTERN] Reference to the user's string table.
extern dsm_stab *g_str_tab;


/*
 *******************************************************************************
 *                        Internal Function Definitions                        *
 *******************************************************************************
*/


// [HTAB]: Hashing routine for the semaphore hash table.
static unsigned int func_hash (void *key) {
    const char *sem_id = (const char *)key;
    return DJBHash(key, strlen(sem_id));
}

// [HTAB]: Freeing routine for the semaphore hash table data.
static void func_free (void *data) {
    dsm_sem_t *sem = (dsm_sem_t *)data;
    free(sem);
}

// [HTAB]: Debugging/printing routines for the semaphore hash table data.
static void func_show (void *data) {
    dsm_sem_t *sem = (dsm_sem_t *)data;
    const char *sem_id = dsm_getStringTableEntry(g_str_tab, sem->sem_id);
    printf("ID: %s, Value: %u", sem_id, sem->value);
}

// [HTAB]: Key to data comparison routine for the semaphore hash table.
static int func_comp (void *key, void *data) {
    dsm_sem_t *sem = (dsm_sem_t *)data;
    const char *sem_id = dsm_getStringTableEntry(g_str_tab, sem->sem_id);

    return (strcmp((char *)key, sem_id) == 0);
}


/*
 *******************************************************************************
 *                            Function Definitions                             *
 *******************************************************************************
*/


// Initializes semaphore hash table. Returns pointer. Exits fatally on error.
dsm_sem_htab *dsm_initSemHashTable (void) {
    return dsm_initHashTable(DSM_SEM_HTAB_LENGTH, func_hash, func_free, 
        func_show, func_comp);
}


// Registers new semaphore in table. Returns pointer. Exits fatally on error.
dsm_sem_t *dsm_setSemHashTableEntry (dsm_sem_htab *htab, char *sem_name) {
    dsm_sem_t *sem;

    // Allocate semaphore. 
    if ((sem = malloc(sizeof(dsm_sem_t))) == NULL) {
        dsm_panic("dsm_setSemHashTableEntry: Allocation failed!");
    }

    // Configure.
    sem->sem_id = dsm_setStringTableEntry(g_str_tab, sem_name);
    sem->value = 1;

    return dsm_setHashTableEntry(htab, sem_name, sem);
}
