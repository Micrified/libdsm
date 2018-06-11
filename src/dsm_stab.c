#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "dsm_stab.h"
#include "dsm_util.h"


/*
 *******************************************************************************
 *                            Function Definitions                             *
 *******************************************************************************
*/


// Allocates and returns a string-table of given size.
dsm_stab *dsm_initStringTable (size_t size) {
    dsm_stab *stab;

    // Allocate string-table.
    if ((stab = malloc(sizeof(dsm_stab))) == NULL) {
        dsm_panic("dsm_initStringTable: Allocation failure!");
    }

    // Set table.
    *stab = (dsm_stab){
        .size = size,
        .sp = 0,
        .tab = dsm_zalloc(size)
    };

    return stab;
}

// Registers string in table. Exits fatally on error.
int dsm_setStringTableEntry (dsm_stab *stab, const char *s) {
    int idx = -1;
    size_t len;

    // Verify arguments.
    if (stab == NULL || stab->tab == NULL || s == NULL) {
        dsm_cpanic("dsm_setStringTableEntry", "NULL parameters!");
    } else {
        len = strlen(s);
    }

    // Keep resizing table until large enough, if needed.
    while (stab->sp + len + 1 >= stab->size) {
        stab->size *= 2;
        if ((stab->tab = realloc(stab->tab, stab->size)) == NULL) {
            dsm_panic("dsm_setStringTableEntry: Realloc failed!");
        }
    }

    // Save string-index.
    idx = stab->sp;

    // Write string to table.
    sprintf(stab->tab + stab->sp, "%s", s);

    // Increment pointer. Include null character.
    stab->sp += len + 1;

    // Return index.
    return idx;
}

// Returns string in table. Returns NULL if invalid index.
const char *dsm_getStringTableEntry (dsm_stab *stab, off_t idx) {

    // Verify arguments.
    if (stab == NULL || stab->tab == NULL) {
        dsm_cpanic("dsm_getStringTableEntry", "NULL argument!");
    }

    // Return NULL if index is invalid. 
    return (idx < stab->sp) ? (stab->tab + idx) : NULL;
}

// [DEBUG] Prints the string-table.
void dsm_showStringTable (dsm_stab *stab) {

    // Verify arguments.
    if (stab == NULL || stab->tab == NULL) {
        dsm_cpanic("dsm_printStringTable", "NULL argument!");
    }

    printf("stab = {");
    for (int i = 0; i < stab->sp; i++) {
        if (stab->tab[i] == '\0') {
            printf("\\0");
        } else {
            putchar(stab->tab[i]);
        }
    }
    printf("}\n");
}

// Frees given string-table. 
void dsm_freeStringTable (dsm_stab *stab) {
    if (stab == NULL) {
        return;
    }
    free(stab->tab);
    free(stab);
}