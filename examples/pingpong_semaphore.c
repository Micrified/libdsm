#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dsm/dsm.h"

/* Make then run the server with: "./bin/dsm_server 4200 2" */

// Configuration struct: dsm_arbiter.h
dsm_cfg cfg = {
    .nproc = 2,             // Total number of expected processes.
    .sid = "Foo",           // Session-identifier: Doesn't do anything here.
    .d_addr = "127.0.0.1",  // Daemon-address. TEMP: Used for server address.
    .d_port = "4200",       // Daemon-port. TEMP: Used for server port.
    .map_size = 4096        // Size of shared memory to reserve.
};

int main (void) {

    // Each process must call dsm_init at least once. Fork before call.
    if (fork() == -1) {
        fprintf(stderr, "Oh no! Time to reboot!\n");
        exit(EXIT_FAILURE);
    }

    // Initialize shared memory system. We don't need the memory here.
    dsm_init(&cfg);

    // Down once if process zero. Down twice if process one to block.
    if (dsm_getgid() == 0) {
        dsm_wait_sem("sem_zero"); // Limited to 32-char. Creates if unique.
    } else {
        dsm_wait_sem("sem_one"); // Down once.
        dsm_wait_sem("sem_one"); // Down twice, now blocked!
    }

    // Play ping pong.
    for (int i = 0; i < 5; i++) {

        if (dsm_getgid() == 0) {
            printf("Ping! ...\n");
        } else {
            printf("... Pong!\n");
        }

        // Unlock each others semaphore.
        if (dsm_getgid() == 0) {
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