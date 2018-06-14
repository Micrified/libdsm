# DSM 

DSM, for Distributed Shared Memory, is a set of programs that enables processes to share memory between machines with similar hardware. 

The constituent programs are
1. A session daemon. 
2. A session server.
3. An arbiter (local server).

The system guarantees atomic access to primitive types (anything up to quads) in the shared memory space, and does not require explicit sychronization or messaging. Simply writing to the shared memory map is sufficient. Networked semaphores provide access control to larger data structures, and barrier directives guarantee process synchronization.

This system is not multi-threaded. Userland multithreading libraries will result in undefined results. Attempt this at your own risk.

### Dependencies

The following specifications must be met to ensure proper operation:
1. x86-64 only. Do not run on any other architecture.
2. Linux only (oldest supported version unknown). Will check. 
3. Intel XED disassembler library must be installed.

### Installation

A makefile is provided to facilitate installation. The following instructions will be of interest:

* `make all`: Compiles the local client. Generates the static library: "libdsm.a"
* `make install`: Installs static library in: `/usr/local/lib` and headers in `/usr/local/include/dsm`.
* `make server`: Compiles session-server. See Usage for how you use this.

### Usage

**Note: If things don't seem to work anymore, execute**: `make clean`!

(Or alternatively, delete `/dev/shm/dsm_file` and `/dev/shm/sem.dsm_start`)


All programs must be compiled with the following linked libraries (in order):

---
`gcc <flags> -ldsm -lpthread -lrt -lxed`
---

In order to run anything, the session-server must be started. It is located in
`/bin/dsm_server` if you ran `make server`. It currently takes a port and the number
of processes to expect.
---
`./bin/dsm_server <port> <nproc>`
---

The server dies every time the session ends. So you'll have to restart it each time you want to run your session again. See the examples folder for how you can instruct the client to connect to the server. Don't worry, the daemon will be in shortly!

**See the examples folder for working programs.**

### Problems

I've logged known problems in the issues tab at: https://github.com/Micrified/dsm.


### Uninstallation

Uninstall with `make uninstall`.
