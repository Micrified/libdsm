# DSM 

DSM, for Distributed Shared Memory, is a set of programs that enables processes to share memory between machines with similar hardware. 

The constituent programs are
1. A session daemon. 
2. A session server.
3. An arbiter (local server).

The system guarantees atomic access to primitive types (anything up to quads) in the shared memory space, and does not require explicit sychronization or messaging. Simply writing to the shared memory map is sufficient. Networked semaphores provide access to control to larger data structures, and barrier directives guarantee process synchronization.

This system is not multi-threaded. Userland multithreading libraries will result in undefined results. Attempt this at your own risk.

### Dependencies

The following specifications must be met to ensure proper operation:
1. x86-64 only. Do not run on any other architecture.
2. Linux only (oldest supported version unknown). Will check. 
3. Intel XED disassembler library must be installed.