#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <ucontext.h>

#include "xed/xed-interface.h"

#include "dsm_sync.h"
#include "dsm_msg.h"
#include "dsm_util.h"
#include "dsm_inet.h"


/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/


// Length of the UD2 instruction for isa: x86-64.
#define UD2_SIZE	2


/*
 *******************************************************************************
 *                              Global Variables                               *
 *******************************************************************************
*/


// Intel XED machine state.
xed_state_t g_xed_machine_state;

// Instruction buffer.
unsigned char g_inst_buf[UD2_SIZE];

// UD2 instruction opcodes for isa: x86-64.
unsigned char g_ud2_opcodes[UD2_SIZE] = {0x0f, 0x0b};

// Pointer to memory address at which fault occurred. 
void *g_fault_addr;


/*
 *******************************************************************************
 *                          Message Wrapper Functions                          *
 *******************************************************************************
*/


// Sends a message to target file-descriptor. Performs packing task.
static void send_msg (int fd, dsm_msg *mp) {
    unsigned char buf[DSM_MSG_SIZE];

    // Pack message.
    dsm_pack_msg(mp, buf);

    // Send message.
    dsm_sendall(fd, buf, DSM_MSG_SIZE);
}

// Receives a message and configures the target message pointer.
static void recv_msg (int fd, dsm_msg *mp) {
    unsigned char buf[DSM_MSG_SIZE];

    // Receive message.
    if (dsm_recvall(fd, buf, DSM_MSG_SIZE) != 0) {
        dsm_cpanic("recv_msg", "Lost connection to host!");
    }

    // Unpack message.
    dsm_unpack_msg(mp, buf);
}


/*
 *******************************************************************************
 *                        Private Function Definitions                         *
 *******************************************************************************
*/


// Performs ILD on given address for decoder state. Returns length in bytes.
static xed_uint_t getInstLength (void *addr, xed_state_t *decoderState) {
	static xed_decoded_inst_t xedd;
	xed_error_enum_t err;

	// Configure decoder for specified machine state.
	xed_decoded_inst_zero_set_mode(&xedd, decoderState);

	// Perform instruction-length-decoding.
	if ((err = xed_ild_decode(&xedd, addr, XED_MAX_INSTRUCTION_BYTES))
		!= XED_ERROR_NONE) {
		dsm_panic(xed_error_enum_t2str(err));
	}

	// Return length.
	return xed_decoded_inst_get_length(&xedd);
}

// Prepares to write: Messages the arbiter, waits for an acknowledgement.
static void takeAccess (void) {

	// Configure message.
	dsm_msg msg = {.type = DSM_MSG_REQ_WRT};
	msg.proc.pid = getpid();

	// Send message to arbiter.
	send_msg(g_sock_io, &msg);

	// Wait for response from arbiter.
	recv_msg(g_sock_io, &msg);

	// Verify message.
	ASSERT_COND(msg.type == DSM_MSG_WRT_NOW && msg.proc.pid == getpid());
}

// Releases access: Messages the arbiter, then suspends itself until continued.
static void dropAccess (void) {
	dsm_msg msg = {.type = DSM_MSG_WRT_DATA};

	// Configure message: Size assumed to be 64 bits.
	msg.data.offset = (uintptr_t)g_fault_addr - (uintptr_t)g_shared_map;
	msg.data.size = sizeof(int64_t);
	memcpy(msg.data.bytes, g_fault_addr, msg.data.size);

	// Send mesage.
	send_msg(g_sock_io, &msg);
	
	// Suspend self until continued.
	if (kill(getpid(), SIGTSTP) == -1) {
		dsm_panic("Couldn't suspend process!\n");
	}
}


/*
 *******************************************************************************
 *                         Public Function Definitions                         *
 *******************************************************************************
*/


// Initializes the decoder tables necessary for use in the sync handlers.
void dsm_sync_init (void) {

	// Initialize decoder table.
	xed_tables_init();

	// Setup machine state.
	xed_state_init2(&g_xed_machine_state, XED_MACHINE_MODE_LONG_64,
		XED_ADDRESS_WIDTH_64b);
}

// Handler: Synchronization action for SIGSEGV.
void dsm_sync_sigsegv (int signal, siginfo_t *info, void *ucontext) {
	ucontext_t *context = (ucontext_t *)ucontext;
	void *prgm_counter = (void *)context->uc_mcontext.gregs[REG_RIP];
	xed_uint_t len;
	UNUSED(signal);

	//printf("[%d] SIGSEGV (%p)!\n", getpid(), info->si_addr);

	// Set fault address.
	g_fault_addr = info->si_addr;

	// Verify address is within shared page. Otherwise panic.
	if ((uintptr_t)g_fault_addr < (uintptr_t)g_shared_map || 
		(uintptr_t)g_fault_addr >= (uintptr_t)g_shared_map + g_map_size) {
		dsm_panicf("Segmentation Fault: %p", g_fault_addr);
	}

	// Grab semaphore and request write access.
	takeAccess();

	// Get instruction length.
	len = getInstLength(prgm_counter, &g_xed_machine_state);

	// Compute start of next instruction.
	void *nextInst = (void *)((uintptr_t)prgm_counter + len);

	// Copy out UD2_SIZE bytes for fault substitution.
	memcpy(g_inst_buf, nextInst, UD2_SIZE);

	// Assign full access permissions to program text page.
	off_t offset = (uintptr_t)nextInst % (uintptr_t)DSM_PAGESIZE;
	void *pageStart = (void *)((uintptr_t)nextInst - offset);
	dsm_mprotect(pageStart, DSM_PAGESIZE, PROT_READ|PROT_WRITE|PROT_EXEC);

	// Copy in the UD2 instruction.
	memcpy(nextInst, g_ud2_opcodes, UD2_SIZE);

	// Give protected portion of shared page read-write access.
	dsm_mprotect(g_shared_map, g_map_size, PROT_WRITE);
}

// Handler: Synchronization action for SIGILL.
void dsm_sync_sigill (int signal, siginfo_t *info, void *ucontext) {
	ucontext_t *context = (ucontext_t *)ucontext;
	void *prgm_counter = (void *)context->uc_mcontext.gregs[REG_RIP];
	UNUSED(signal);
	UNUSED(info);

	// Verify fault address is set. If not, abort.
	if (g_fault_addr == NULL) {
		dsm_panicf("Illegal Instruction (SIGILL). Aborting!");
	}

	//printf("SIGILL!\n"); fflush(stdout);

	// Restore origin instruction.
	memcpy(prgm_counter, g_inst_buf, UD2_SIZE);

	// Protect shared page again.
	dsm_mprotect(g_shared_map, g_map_size, PROT_READ);

	// Release lock and send sychronization information.
	dropAccess();

	// Unset fault address.
	g_fault_addr = NULL;
}
