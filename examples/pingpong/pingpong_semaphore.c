#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dsm/dsm.h"

int main (void) {

	// Initializer shared memory. Forking is automatic.
	dsm_init("pingpong", 2, 2, 4096);

    // Down once if process zero. Down twice if process one to block.
    if (dsm_get_gid() == 0) {
        dsm_wait_sem("sem_zero"); // Limited to 32-char. Creates if unique.
    } else {
        dsm_wait_sem("sem_one"); // Down once.
        dsm_wait_sem("sem_one"); // Down twice, now blocked!
    }

    // Play ping pong.
    for (int i = 0; i < 5; i++) {

        if (dsm_get_gid() == 0) {
            printf("Ping! ...\n");
        } else {
            printf("... Pong!\n");
        }

        // Unlock each others semaphore.
        if (dsm_get_gid() == 0) {
            dsm_post_sem("sem_one");
            dsm_wait_sem("sem_zero");
        } else {
            dsm_post_sem("sem_zero");
            dsm_wait_sem("sem_one");
        }
    }

    // Unblock for exit.
    dsm_post_sem("sem_one");

    // De-initialize the shared map.
    dsm_exit();
}