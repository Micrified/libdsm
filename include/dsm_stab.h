#if !defined(DSM_STAB_H)
#define DSM_STAB_H

#include <stdlib.h>


/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/


#define DSM_STR_TAB_SIZE        256


/*
 *******************************************************************************
 *                              Type Definitions                               *
 *******************************************************************************
*/


// Type describing a string-table.
typedef struct dsm_stab {
    size_t size;            // String-table size.
    off_t sp;                 // Pointer to free byte space.
    char *tab;              // String-table.
} dsm_stab;


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// Allocates and returns a string-table of given size.
dsm_stab *dsm_initStringTable (size_t size);

// Registers string in table. Exits fatally on error.
int dsm_setStringTableEntry (dsm_stab *stab, const char *s);

// Returns string in table. Returns NULL if invalid index.
const char *dsm_getStringTableEntry (dsm_stab *stab, off_t idx);

// [DEBUG] Prints the string-table.
void dsm_printStringTable (dsm_stab *stab);

// Frees given string-table. 
void dsm_freeStringTable (dsm_stab *stab);


#endif