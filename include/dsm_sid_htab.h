#if !defined(DSM_SID_HTAB_H)
#define DSM_SID_HTAB_H

#include "dsm_htab.h"


/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/


// Number of buckets in the session hash-table.
#define DSM_SID_HTAB_LENGTH		16


/*
 *******************************************************************************
 *                              Type Definitions                               *
 *******************************************************************************
*/


// Type describing a session.
typedef struct dsm_sid_t {
	int sid;					// Session identifier.
	int port;					// Session port.
} dsm_sid_t;


// Custom redefinition of dsm_htab
typedef dsm_htab dsm_sid_htab;


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// Initializes session hash table. Returns pointer. Exits fatally on error.
dsm_sid_htab *dsm_initSIDHashTable (void);

// Registers new session in table. Returns pointer. Exits fatally on error.
dsm_sid_t *dsm_setSIDHashTableEntry (dsm_sid_htab *htab, char *sid_name);



#endif