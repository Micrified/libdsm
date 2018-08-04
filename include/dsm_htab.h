#if !defined (DSM_HTAB_H)
#define DSM_HTAB_H


/*
 *******************************************************************************
 *                              Type Definitions                               *
 *******************************************************************************
*/


// Type describing a generic hash-table entry.
typedef struct dsm_htab_entry {
    void *data;                          // Data pointer.
    struct dsm_htab_entry *next;         // Next linked entry.
} dsm_htab_entry;

// Type describing a generic hash-table.
typedef struct dsm_htab {
	unsigned int (*func_hash)(void *);   // Hashing function for key.
	void (*func_free)(void *);           // Data freeing function.
	void (*func_show)(void *);           // Data printing function.
	int (*func_comp)(void *, void *);    // Data comparison function.
	dsm_htab_entry **tab;                // Hash-table p Putointer.
	size_t length;                       // Hash-table bucket count.
} dsm_htab;


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// Initializes the hash table. Returns pointer. Exits fatally on error.
dsm_htab *dsm_initHashTable (
	size_t length,
	unsigned int (*func_hash)(void *),
	void (*func_free)(void *),
	void (*func_show)(void *),
	int (*func_comp)(void *, void *));

// Registers data in table. Returns pointer to data. Exits fatally on error.
void *dsm_setHashTableEntry (dsm_htab *htab, void *key, void *data);

// Retrieves data from table. Returns pointer or NULL if no entry found.
void *dsm_getHashTableEntry (dsm_htab *htab, void *key);

// Removes (frees) data from table. Exits on error.
void dsm_remHashTableEntry (dsm_htab *htab, void *key);

// Flushes hash table.
void dsm_flushHashTable (dsm_htab *htab);

// Prints hash table.
void dsm_showHashTable (dsm_htab *htab);

// Frees hash table.
void dsm_freeHashTable (dsm_htab *htab);


#endif