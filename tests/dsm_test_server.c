#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dsm_msg.h"
#include "dsm_util.h"
#include "dsm_inet.h"

// Socket connected to the server.
int sock;

// Receive a message and verify it.
void recv_message (void) {
    unsigned char buf[DSM_MSG_SIZE];
    dsm_msg msg;

    // Read in message.
    dsm_recvall(sock, buf, DSM_MSG_SIZE);

    // Unpack message.
    dsm_unpack_msg(&msg, buf);

    // Print message.
    dsm_showMsg(&msg);
}

// Send a message.
void send_message (int option) {
    dsm_msg msg = {0};
    unsigned char buf[DSM_MSG_SIZE];

    switch (option) {
        case 1:
            msg.type = DSM_MSG_ADD_PID;
            printf("pid: "); scanf("%d", &msg.proc.pid);
            break;
        
        case 2: 
            msg.type = DSM_MSG_REQ_WRT;
            printf("pid: "); scanf("%d", &msg.proc.pid);
            break;
        
        case 3: 
            msg.type = DSM_MSG_ALL_STP;
            msg.task.nproc = 1;
            break;
        
        case 4: 
            msg.type = DSM_MSG_WRT_DATA;
            msg.data.offset = 42;
            msg.data.size = 84;
            printf("Got here!\n");
            break;
        
        case 5: 
            msg.type = DSM_MSG_GOT_DATA;
            msg.task.nproc = 1;
            break;
        
        case 6: 
            msg.type = DSM_MSG_HIT_BAR;
            printf("pid: "); scanf("%d", &msg.proc.pid);
            break;
        
        case 7: 
            msg.type = DSM_MSG_POST_SEM;
            printf("pid: "); scanf("%d", &msg.sem.pid);
            printf("sem_name: "); scanf("%s", msg.sem.sem_name);
            break;
        
        case 8: 
            msg.type = DSM_MSG_WAIT_SEM;
            printf("pid: "); scanf("%d", &msg.sem.pid);
            printf("sem_name: "); scanf("%s", msg.sem.sem_name);
            break;
        
        case 9: 
            msg.type = DSM_MSG_EXIT;
            break;
        
    }

    dsm_pack_msg(&msg, buf);
    dsm_sendall(sock, buf, DSM_MSG_SIZE);
    printf("Sent...\n");
}

int main (int argc, const char *argv[]) {
    int option, quit = 0;

    // Verify arguments.
    if (argc != 3) {
        fprintf(stderr, "Usage: ./%s <address> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Connect to socket.
    sock = dsm_getConnectedSocket(argv[1], argv[2]);

    do {
        printf("------- OPTIONS -------\n");
        printf("0. RECV\n");
        printf("1. SEND: DSM_MSG_ADD_PID\n");
        printf("2. SEND: DSM_MSG_REQ_WRT\n");
        printf("3. SEND: DSM_MSG_ALL_STP\n");
        printf("4. SEND: DSM_MSG_WRT_DATA\n");
        printf("5. SEND: DSM_MSG_GOT_DATA\n");
        printf("6. SEND: DSM_MSG_HIT_BAR\n");
        printf("7. SEND: DSM_MSG_POST_SEM\n");
        printf("8. SEND: DSM_MSG_WAIT_SEM\n");
        printf("9. SEND: DSM_MSG_EXIT\n");
        printf("Input: "); scanf("%d", &option);
        if (option == 0) {
            recv_message();
        } else if (option > 9) {
            quit = 1;
        } else {
            send_message(option);
        }
    } while (!quit);
    return 0;
}