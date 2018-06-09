#if !defined (DSM_HTAB_H)
#define DSM_HTAB_H


/*
 *******************************************************************************
 *                              Type Definitions                               *
 *******************************************************************************
*/


// Type describing a generic hash-table entry.
typedef struct dsm_htab_entry {
    int key;                        // String-table index.
    void *data;                     // Data pointer.
    struct dsm_htab_entry *next;    // Next linked entry.
} dsm_htab_entry;


// Type describing a generic hash-table.
typedef struct dsm_htab {
    void (*func_showData)(void *);  // Data printing function.
    void (*func_freeData)(void *);  // Data freeing function.
    dsm_htab_entry **tab;           // Hash-table pointer.
    int length;                     // Length of hash-table.
} dsm_htab;


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// Initializes the hash table. Returns hash-table pointer.
dsm_htab *dsm_initHashTable (int length,
    void (*func_showData)(void *), void (*func_freeData)(void *));

// Prints all table entries. 
void dsm_showHashTable (dsm_htab *htab);

// Returns data for given key. Returns NULL if no entry exists.
void *dsm_getHashTableEntry (dsm_htab *htab, int hash, int key);

// Inserts a new table entry. Key must be string table index. Returns pointer.
void *dsm_setHashTableEntry (dsm_htab *htab, int hash, int key, void *data);

// Removes a table entry. Uses supplied function to free data. Exits on error.
void dsm_removeHashTableEntry (dsm_htab *htab, int hash, int key);

// Flushes the entire table.
void dsm_flushHashTable (dsm_htab *htab);

// Free's the hash table.
void dsm_freeHashTable (dsm_htab *htab);


#endif