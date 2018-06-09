#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include "dsm_stab.h"

const char *names[] = {"Dennis Ritchie", "Kenneth Thompson", "Robert Pike", "David Presotto", "Phillip Winterbottom"};
off_t seq_offsets[] = {0, 15, 32, 44, 59};

int main (void) {
    dsm_stab *stab = dsm_initStringTable(4);
    assert(stab != NULL && (stab->tab != NULL) && stab->size == 4);

    // Register some strings: Verify sequential offsets are correct.
    for (int i = 0; i < 5; i++) {
        assert(dsm_setStringTableEntry(stab, names[i]) == seq_offsets[i]);
    }

    // Print the table.
    //dsm_printStringTable(stab);

    // Verify strings match.
    for (int i = 0; i < 5; i++) {
        //printf("Does \"%s\" == \"%s\"?\n", names[i], dsm_getStringTableEntry(stab, seq_offsets[i]));
        assert(strcmp(names[i], dsm_getStringTableEntry(stab, seq_offsets[i])) == 0);
    }

    dsm_freeStringTable(stab);

    return 0;
}