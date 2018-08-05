#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dsm/dsm.h"

int main (void) {
    int *turn;

	// Initialize shared memory. Forking is automatic. Returns map pointer.
	turn = (int *)dsm_init("pingpong", 2, 2, 4096);

    // Play ping pong.
    for (int i = 0; i < 5; i++) {
        while (*turn != dsm_get_gid());
        if (dsm_get_gid() == 0) {
            printf("Ping! ...\n");
        } else {
            printf("... Pong!\n");
        }
        *turn = (1 - *turn);
    }

    // De-initialize the shared map.
    dsm_exit();

    return 0;
}
