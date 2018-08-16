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


// Creates a hole. Returns positive ID on success, or -1 on error.
int dsm_new_hole (off_t offset, size_t size, dsm_hole **holes);

// Frees a linked list of memory holes
void dsm_free_holes (dsm_hole *holes);

/* Returns pointer to a hole if the memory space resides inside one.
 * Ext is added to the compared hole size. It allows accesses to the last
 * byte of the hole to not count as outside the hole.
*/
dsm_hole *dsm_in_hole (off_t offset, size_t size, size_t ext, dsm_hole *holes);

// Returns a pointer to a hole given a hole identifier. Returns NULL on error.
dsm_hole *dsm_get_hole (int id, dsm_hole *holes);

// Returns nonzero if given memory range overlaps with a hole. 
int dsm_overlaps_hole (off_t offset, size_t size, dsm_hole *holes);

// Removes a hole. Returns nonzero on error.
int dsm_del_hole (int id, dsm_hole **holes);

// [DEBUG] Prints all holes to output.
void dsm_show_holes (dsm_hole *holes);


#endif
