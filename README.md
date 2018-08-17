# LIBDSM 

This repository contains a DSM in the form of a drag and drop library. The abbreviation "DSM" stands for Distributed Shared Memory. A DSM system allows processes across different machines to share a logically identical memory space. This allows variables to be shared transparently.

The following programs compose the DSM library:
1. A session daemon. 
2. A session server.
3. An arbiter (local server).

The system guarantees atomic write access to primitive types (anything up to quads) in the shared memory space, and does not require explicit sychronization or messaging. Simply writing to the shared memory map is sufficient. Networked semaphores provide access control to larger data structures, and barrier directives guarantee process synchronization. Finally, memory holes provide a means to perform bulk changes with minimal overhead.

This system will not work with multi-threaded processes. Userland multithreading libraries will result in undefined results. Attempt this at your own risk.

### Dependencies

The following specifications must be met to ensure proper operation:
1. x86-64 only. Do not run on any other architecture.
2. Linux only (oldest supported version unknown). Will check. 
3. Intel XED disassembler library must be installed.
4. Hardware sharing memory space must have the same memory model.

### Installation

A makefile facilitates installation. The following instructions are of interest:
* `make all`: Compiles and generates static library: "libdsm.a"
* `make install`: Installs `libdsm.a` in `/usr/local/lib` and headers in `/usr/local/include/dsm`.
    Also installs `dsm_arbiter`, `dsm_server`, and `dsm_daemon` in `/usr/local/bin`.

### Usage

All programs must be compiled with the following linked libraries (in order):

---

`gcc <flags> -ldsm -lpthread -lrt -lxed`

---

A template program is available in examples/template. 

In order for the library to work, the session-daemon must be running. If you have invoked `make install`, it will be in `/usr/local/bin`. As long as that is in your PATH variable, you may start it with: `dsm_daemon`. 

**See the examples folder for working programs.**

### Problems

I've logged known problems in the issues tab at: https://github.com/Micrified/libdsm.


### Uninstallation

Uninstall with `make uninstall`.
