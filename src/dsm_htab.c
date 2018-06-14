#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsm_htab.h"
#include "dsm_util.h"

/*
 *******************************************************************************
 *                        Private Function Definitions                         *
 *******************************************************************************
*/


static void *getHashTableEntry (dsm_htab_entry *entry, void *key,
	int (*func_comp)(void *, void *)) {

	// Return NULL if not found.
	if (entry == NULL) {
		return NULL;
	}

	// Return data if comparator returns nonzero.
	if (func_comp(key, entry->data) != 0) {
		return entry->data;
	}

	// Search next entry.
	return getHashTableEntry(entry->next, key, func_comp);
}

static void *remHashTableEntry (dsm_htab_entry *entry, void *key,
	int (*func_comp)(void *, void *), void (*func_free)(void *)) {
	dsm_htab_entry *next;

	// Return NULL if not found.
	if (entry == NULL) {
		return NULL;
	}

	// Free entry and return next if comparator returns nonzero.
	if (func_comp(key, entry->data) != 0) {
		next = entry->next;
		func_free(entry->data);
		free(entry);
		return next;
	}

	// Set next entry as result of recursive call.
	entry->next = remHashTableEntry(entry->next, key, func_comp, func_free);
	return entry;
}

static void showHashTableEntry (dsm_htab_entry *entry, 
	void (*func_show)(void *)) {

	// Recursive guard.
	if (entry == NULL) {
		printf("{NULL}\n");
		return;
	}

	// Print current node, then next.
	printf("-> {"); func_show(entry->data); printf("}");
	showHashTableEntry(entry->next, func_show);
}

static void *freeHashTableEntry (dsm_htab_entry *entry, 
	void (*func_free)(void *)) {

	// Recursive guard.
	if (entry == NULL) {
		return NULL;
	}

	// Frees linked elements first, then itself.
	freeHashTableEntry(entry->next, func_free);
	func_free(entry->data);
	free(entry);
	return NULL;
}


/*
 *******************************************************************************
 *                         Public Function Definitions                         *
 *******************************************************************************
*/


// Initializes the hash table. Returns pointer. Exits fatally on error.
dsm_htab *dsm_initHashTable (
	int length,
	unsigned int (*func_hash)(void *),
	void (*func_free)(void *),
	void (*func_show)(void *),
	int (*func_comp)(void *, void *)) {
	dsm_htab *htab;

	// Verify arguments.
	if (func_hash == NULL || func_free == NULL || 
		func_show == NULL || func_comp == NULL) {
		dsm_cpanic("dsm_initHashTable", "NULL argument!");
	}

	// Allocate table.
	if ((htab = malloc(sizeof(dsm_htab))) == NULL) {
		dsm_panic("dsm_initHashTable: Allocation failure!");
	}

	// Configure table.
	*htab = (dsm_htab) {
		.func_hash = func_hash,
		.func_free = func_free,
		.func_show = func_show,
		.func_comp = func_comp,
		.tab = dsm_zalloc(length * sizeof(dsm_htab_entry *)),
		.length = length
	};

	return htab;
}

// Registers data in table. Returns pointer to data. Exits fatally on error.
void *dsm_setHashTableEntry (dsm_htab *htab, void *key, void *data) {
	dsm_htab_entry *entry = NULL;

	// Compute hash.
	unsigned int hash = htab->func_hash(key) % htab->length;

	// Verify entry doesn't already exist.
	if ((entry = dsm_getHashTableEntry(htab, key)) != NULL) {
		dsm_cpanic("dsm_setHashTableEntry", "Entry already exists!");
	}

	// Allocate new entry.
	if ((entry = malloc(sizeof(dsm_htab_entry))) == NULL) {
		dsm_panic("dsm_setHashTableEntry: Allocation failure!");
	}

	// Configure entry: Set next as current list-head.
	*entry = (dsm_htab_entry) {
		.data = data,
		.next = htab->tab[hash]
	};
	
	// Insert at head of list. Return data attribute.
	htab->tab[hash] = entry;
	return entry->data;
}

// Retrieves data from table. Returns pointer or NULL if no entry found.
void *dsm_getHashTableEntry (dsm_htab *htab, void *key) {

	// Compute hash.
	unsigned int hash = htab->func_hash(key) % htab->length;

	// Verify arguments.
	ASSERT_COND(htab != NULL && htab->func_comp != NULL);

	// Search for entry.
	return getHashTableEntry(htab->tab[hash], key, htab->func_comp);
}

// Removes (frees) data from table. Exits on error.
void dsm_remHashTableEntry (dsm_htab *htab, void *key) {

	// Compute hash.
	unsigned int hash = htab->func_hash(key) % htab->length;

	// Reset list without entry.
	htab->tab[hash] = remHashTableEntry(htab->tab[hash], key, htab->func_comp,
		htab->func_free);
}

// Flushes hash table.
void dsm_flushHashTable (dsm_htab *htab) {
	for (int i = 0; i < htab->length; i++) {
		freeHashTableEntry(htab->tab[i], htab->func_free);
		htab->tab[i] = NULL;
	}
}

// Prints hash table.
void dsm_showHashTable (dsm_htab *htab) {
	for (int i = 0; i < htab->length; i++) {
		printf("%d: ", i);
		showHashTableEntry(htab->tab[i], htab->func_show);
	}
}

// Frees hash table.
void dsm_freeHashTable (dsm_htab *htab) {
	dsm_flushHashTable(htab);
	free(htab->tab);
	free(htab);
}



