#if !defined(DSM_ARBITER_H)
#define DSM_ARBITER_H

/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/


// The default port at which the arbiter can be reached.
#define DSM_ARB_PORT	"4800"


/*
 *******************************************************************************
 *                              Type Definitions                               *
 *******************************************************************************
*/


// Session configuration structure.
typedef struct dsm_cfg {
    unsigned int nproc;         // Total number of expected processes.
    const char *sid;            // Session identifier.
    const char *d_addr;         // Daemon address.
    const char *d_port;         // Daemon port.
    size_t map_size;            // Desired memory map size (page multiple).
} dsm_cfg;


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// Runs the arbiter main loop. Exits on success.
void dsm_arbiter (dsm_cfg *cfg);


#endif