#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsm_util.h"
#include "dsm_htab.h"


/*
 *******************************************************************************
 *                        Private Function Definitions                         *
 *******************************************************************************
*/


// Uses the supplied function to print the linked list of entries.
static void showHashTableEntry (dsm_htab_entry *entry, 
    void (*func_showData)(void *)) {

    // Return if entry is NULL.
    if (entry == NULL) {
        printf("{NULL}\n");
        return;
    }

    // Print self first, then next element.
    printf("{ key: %d, ", entry->key);
    func_showData(entry->data);
    printf("} -> ");

    // Show next entry.
    showHashTableEntry(entry->next, func_showData);
}

// Returns data field of target entry. Or NULL if it isn't found.
static void *getHashTableEntry (dsm_htab_entry *entry, int key) {
    
    // Return NULL if entry isn't found.
    if (entry == NULL) {
        return NULL;
    }

    // Return data if key matches that of current entry.
    if (entry->key == key) {
        return entry->data;
    }

    // Search the next entry.
    return getHashTableEntry(entry->next, key);
}

// Free's entry for given key in linked entry list. Returns new head pointer.
static void *removeHashTableEntry (dsm_htab_entry *entry, int key, 
    void (*func_freeData)(void *)) {
    void *ptr;

    // Return NULL if at end of list.
    if (entry == NULL) {
        return NULL;
    }

    // If current entry, then remove it and return.
    if (entry->key == key) {
        ptr = entry->next;
        func_freeData(entry->data);
        free(entry);
        return ptr;
    }

    // Set next entry as return value of recursive function.
    entry->next = removeHashTableEntry(entry->next, key, func_freeData);
    return entry;
}

// Frees a linked list of entries. Returns NULL.
static void *freeHashTableEntry (dsm_htab_entry *entry,
    void (*func_freeData)(void *)) {

    // Return if NULL.
    if (entry == NULL) {
        return NULL;
    }

    // Free next entry first.
    freeHashTableEntry(entry->next, func_freeData);
    func_freeData(entry->data);
    free(entry);
    return NULL;
}


/*
 *******************************************************************************
 *                         Public Function Definitions                         *
 *******************************************************************************
*/


// Initializes the hash table. Returns hash-table pointer.
dsm_htab *dsm_initHashTable (int length,
    void (*func_showData)(void *), void (*func_freeData)(void *)) {
    dsm_htab *htab;

    // Verify arguments.
    if (func_showData == NULL || func_freeData == NULL) {
        dsm_panic("dsm_initHashTable failed: NULL arguments!");
    }

    // Allocate htab instance.
    if ((htab = malloc(sizeof(dsm_htab))) == NULL) {
        dsm_panic("dsm_initHashTable failed: Couldn't allocate type!");
    }

    // Configure htab and return.
    *htab = (dsm_htab) {
        .func_showData = func_showData,
        .func_freeData = func_freeData,
        .tab = (dsm_htab_entry **)dsm_zalloc(length * sizeof(dsm_htab_entry *)),
        .length = length
    };

    return htab;
}

// Prints all table entries. 
void dsm_showHashTable (dsm_htab *htab) {

    // Verify argument.
    if (htab == NULL || htab->tab == NULL || htab->func_showData == NULL) {
        dsm_cpanic("dsm_freeHashTable", "NULL arg or arg field!");
    }

    // Show all linked-list elements.
    for (int i = 0; i < htab->length; i++) {
        showHashTableEntry(htab->tab[i], htab->func_showData);
    }
}

// Returns data for given key. Returns NULL if no entry exists.
void *dsm_getHashTableEntry (dsm_htab *htab, int hash, int key) {
    
    // Verify argument.
    if (htab == NULL || htab->tab == NULL) {
        dsm_cpanic("dsm_getHashTableEntry", "NULL arg or arg field!");
    }

    // Verify hash.
    if (hash < 0 || hash > htab->length) {
        dsm_cpanic("dsm_getHashTableEntry", "Hash out of bounds!");
    }

    // Return entry, or NULL if it doesn't exist.
    return getHashTableEntry(htab->tab[hash], key);
}

// Inserts a new table entry. Key must be string table index. Returns pointer.
void *dsm_setHashTableEntry (dsm_htab *htab, int hash, int key, void *data) {
    dsm_htab_entry *entry;

    // Verify argument.
    if (htab == NULL || htab->tab == NULL) {
        dsm_cpanic("dsm_newHashTableEntry", "NULL arg or arg field!");
    }

    // Verify hash.
    if (hash < 0 || hash > htab->length) {
        dsm_cpanic("dsm_newHashTableEntry", "Hash out of bounds!");
    }

    // Allocate new entry.
    if ((entry = malloc(sizeof(dsm_htab_entry))) == NULL) {
        dsm_cpanic("dsm_newHashTableEntry", "Allocation failed!");
    }

    // Configure new entry.
    *entry = (dsm_htab_entry) {
        .key = key,
        .data = data,
        .next = htab->tab[hash]
    };

    // Both set new entry as the head, and return data.
    return (htab->tab[hash] = entry)->data;
}

// Removes a table entry. Uses supplied function to free data. Exits on error.
void dsm_removeHashTableEntry (dsm_htab *htab, int hash, int key) {
    
    // Verify argument.
    if (htab == NULL || htab->tab == NULL) {
        dsm_cpanic("dsm_removeHashTableEntry", "NULL arg or arg field!");
    }

    // Verify hash.
    if (hash < 0 || hash > htab->length) {
        dsm_cpanic("dsm_removeHashTableEntry", "Hash out of bounds!");
    }

    htab->tab[hash] = removeHashTableEntry(htab->tab[hash], key, 
        htab->func_freeData);
}

// Flushes the entire table.
void dsm_flushHashTable (dsm_htab *htab) {

    // Verify argument.
    if (htab == NULL || htab->tab == NULL) {
        dsm_cpanic("dsm_flushHashTable", "NULL arg or arg field!");
    }

    // Free linked-list elements.
    for (int i = 0; i < htab->length; i++) {
        htab->tab[i] = freeHashTableEntry(htab->tab[i], htab->func_freeData);
    }
}

// Free's the hash table.
void dsm_freeHashTable (dsm_htab *htab) {

    // Verify argument.
    if (htab == NULL || htab->tab == NULL) {
        dsm_cpanic("dsm_freeHashTable", "NULL arg or arg field!");
    }

    // Free linked-list elements.
    for (int i = 0; i < htab->length; i++) {
        htab->tab[i] = freeHashTableEntry(htab->tab[i], htab->func_freeData);
    }

    // Free the table.
    free(htab->tab);

    // Free the structure.
    free(htab);
}



