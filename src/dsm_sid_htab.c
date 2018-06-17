#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsm_sid_htab.h"
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


// [HTAB]: Hashing routine for the session hash table.
static unsigned int func_hash (void *key) {
	const char *sid_name = (const char *)key;
	return DJBHash(key, strlen(sid_name));
}

// [HTAB]: Freeing routine for the session hash table.
static void func_free (void *data) {
	dsm_sid_t *session = (dsm_sid_t *)data;
	free(session);
}

// [HTAB]: Debugging/printing routines for the session hash table.
static void func_show (void *data) {
	dsm_sid_t *session = (dsm_sid_t *)data;
	const char *sid_name = dsm_getStringTableEntry(g_str_tab, session->sid);
	printf("ID: %s, Port: %d", sid_name, session->port);
}

// [HTAB]: Key to data comparison routine for the session hash table.
static int func_comp (void *key, void *data) {
	dsm_sid_t *session = (dsm_sid_t *)data;
	const char *sid_name = dsm_getStringTableEntry(g_str_tab, session->sid);
	
	return (strcmp((char *)key, sid_name) == 0);
}


/*
 *******************************************************************************
 *                        Internal Function Definitions                        *
 *******************************************************************************
*/


// Initializes session hash table. Returns pointer. Exits fatally on error.
dsm_sid_htab *dsm_initSIDHashTable (void) {
	return dsm_initHashTable(DSM_SID_HTAB_LENGTH, func_hash, func_free,
		func_show, func_comp);
}

// Registers new session in table. Returns pointer. Exits fatally on error.
dsm_sid_t *dsm_setSIDHashTableEntry (dsm_sid_htab *htab, char *sid_name) {
	dsm_sid_t *session;

	// Allocate session.
	if ((session = malloc(sizeof(dsm_sid_t))) == NULL) {
		dsm_panic("dsm_setSIDHashTableEntry: Allocation failed!");
	}

	// Configure.
	session->sid = dsm_setStringTableEntry(g_str_tab, sid_name);
	session->port = -1;

	return dsm_setHashTableEntry(htab, sid_name, session);
}
