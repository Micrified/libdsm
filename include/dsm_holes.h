#if !defined(DSM_HOLES_H)
#define DSM_HOLES_H

#include <stdlib.h>


/*
 *******************************************************************************
 *                              Type Definitions                               *
 *******************************************************************************
*/


// Type describing a memory hole.
typedef struct dsm_hole {
	int id;                                    // Hole identifier.
	off_t offset;                              // Starting offset of hole.
	size_t size;                               // Size (in bytes) of hole.
	struct dsm_hole *next;                     // Pointer to next node.
} dsm_hole;


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// Digs (creates) a hole. Returns positive ID on success, or -1 on error.
int dsm_dig_hole (off_t offset, size_t size, dsm_hole **holes);

// Frees a linked list of memory holes
void dsm_free_holes (dsm_hole *holes);

// Returns nonzero if given memory range is completely within a hole. 
int dsm_in_hole (off_t offset, size_t size, dsm_hole *holes);

// Returns nonzero if given memory range overlaps with a hole. 
int dsm_overlaps_hole (off_t offset, size_t size, dsm_hole *holes);

// Fills (removes) a hole. Returns nonzero on error.
int dsm_fill_hole (int id, dsm_hole **holes);

// [DEBUG] Prints all holes to output.
void dsm_show_holes (dsm_hole *holes);


#endif
