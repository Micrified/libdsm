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
#include "dsm_signal.h"
#include "dsm_msg_io.h"


/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/


// Length of the UD2 instruction for isa: x86-64.
#define UD2_SIZE		2


/*
 *******************************************************************************
 *                              Global Variables                               *
 *******************************************************************************
*/


// Intel XED machine state.
xed_state_t g_xed_machine_state;

// Instruction buffer.
unsigned char g_inst_buf[UD2_SIZE];

// Copy buffer.
unsigned char g_mem_buf[SYS_ADDR_WIDTH];

// UD2 instruction opcodes for isa: x86-64.
unsigned char g_ud2_opcodes[UD2_SIZE] = {0x0f, 0x0b};

// Pointer to memory address at which fault occurred. 
void *g_fault_addr;

// Boolean: Indicates if access should be synchronized.
unsigned int g_skip_sync;

// Hole last access was in. If NULL, access was not in a hole.
dsm_hole *g_active_hole;


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

	// Temporarily ignore SIGTSTP.
	dsm_sigignore(SIGTSTP);

	// Configure message.
	dsm_msg msg = {.type = DSM_MSG_REQ_WRT};
	msg.proc.pid = getpid();

	// Send message to arbiter.
	dsm_send_msg(g_sock_io, &msg);

	// Wait for response from arbiter.
	dsm_recv_msg(g_sock_io, &msg);;

	// Verify message.
	ASSERT_COND(msg.type == DSM_MSG_WRT_NOW && msg.proc.pid == getpid());
}

// Releases access: Messages the arbiter, then suspends itself until continued.
static void dropAccess (size_t modified_size) {
	dsm_msg msg = {.type = DSM_MSG_WRT_DATA};

	// Reset default behavior for SIGTSTP.
	dsm_sigdefault(SIGTSTP);

	// Compute maximum allowed size (so not to go out of range).
	size_t offset = (size_t)((intptr_t)g_fault_addr - (intptr_t)g_shared_map);
	size_t size = MAX(MIN(modified_size, ((size_t)g_map_size - offset)), 1);

	// Configure message.
	msg.data.offset = offset;
	msg.data.size = size;
	msg.data.buf = g_fault_addr;

	// Send mesage.
	dsm_send_msg(g_sock_io, &msg);

	// Send end of data message.
	msg.type = DSM_MSG_WRT_END;
	dsm_send_msg(g_sock_io, &msg);
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
	off_t offset, fault_offset;
	UNUSED(signal);

	// Set fault address.
	g_fault_addr = info->si_addr;

	// Compute fault offset.
	fault_offset = (intptr_t)g_fault_addr - (intptr_t)g_shared_map;

	// Verify address is within shared page. Otherwise panic.
	if ((intptr_t)g_fault_addr < (intptr_t)g_shared_map || 
		(intptr_t)g_fault_addr >= (intptr_t)g_shared_map + g_map_size) {
		dsm_panicf("Segmentation Fault: %p", g_fault_addr);
	}

	// Determine whether access in hole or not.
	g_active_hole = dsm_in_hole(fault_offset, SYS_ADDR_WIDTH,
		SYS_ADDR_WIDTH, g_shm_holes);

	// Request write access if the addressable range wasn't in a hole.
	if (g_active_hole == NULL) {
		takeAccess();
	}

	// Make copy of memory before modification (do after access granted).
	memcpy(g_mem_buf, g_fault_addr, SYS_ADDR_WIDTH);

	// Get instruction length.
	len = getInstLength(prgm_counter, &g_xed_machine_state);

	// Compute start of next instruction.
	void *nextInst = (void *)((uintptr_t)prgm_counter + len);

	// Copy out UD2_SIZE bytes for fault substitution.
	memcpy(g_inst_buf, nextInst, UD2_SIZE);

	// Assign full access permissions to program text page.
	offset = (uintptr_t)nextInst % (uintptr_t)DSM_PAGESIZE;
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

	// Restore origin instruction.
	memcpy(prgm_counter, g_inst_buf, UD2_SIZE);

	// Protect shared page again.
	dsm_mprotect(g_shared_map, g_map_size, PROT_READ);

	// Compute the size of the modified memory.
	size_t modified_size = dsm_memcmp(g_fault_addr, g_mem_buf, SYS_ADDR_WIDTH);
	
	// Release lock and send synchronization info if needed.
	if (g_active_hole == NULL) {
		dropAccess(modified_size);
	}

	// Unset fault address.
	g_fault_addr = NULL;
}
