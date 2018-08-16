#include <stdio.h>
#include "dsm_holes.h"
#include "dsm_util.h"


/*
 *******************************************************************************
 *                              Global Variables                               *
 *******************************************************************************
*/


// Hole identity counter. 
int g_hole_id;


/*
 *******************************************************************************
 *                        Internal Function Definitions                        *
 *******************************************************************************
*/


// Removes hole with id from linked-list. Sets exists to 1 if it was found.
static dsm_hole *remove_hole (int id, int *exist_p, dsm_hole *holes) {
	dsm_hole *next;

	// If end of list. Return.
	if (holes == NULL) return NULL;

	// If current node is target, free it and return next.
	if (holes->id == id) {
		if (exist_p != NULL) *exist_p = 1;
		next = holes->next;
		free(holes);
		return next;
	}

	// Otherwise return next.
	holes->next = remove_hole(id, exist_p, holes->next);
	return holes;
}


/*
 *******************************************************************************
 *                            Function Definitions                             *
 *******************************************************************************
*/


// Creates a hole. Returns positive ID on success, or -1 on error.
int dsm_new_hole (off_t offset, size_t size, dsm_hole **holes) {
	dsm_hole *hole;

	// Return -1 if hole overlaps another hole.
	if (dsm_overlaps_hole(offset, size, *holes) != 0) {
		return -1;
	}

	// Compute the next ID.
	unsigned int id = g_hole_id++;

	// Allocate hole. Return -1 on error.
	if (holes == NULL || (hole = malloc(sizeof(dsm_hole))) == NULL) {
		return -1;
	}

	// Configure it.
	*hole = (dsm_hole) {
		.id = id,
		.offset = offset,
		.size = size,
		.next = *holes
	};

	// Insert node at head of linked-list.
	*holes = hole;

	// Return identifier.
	return id;
}

// Frees a linked list of memory holes.
void dsm_free_holes (dsm_hole *holes) {
	if (holes == NULL) {
		return;
	}
	dsm_free_holes(holes->next);
	free(holes);
}


/* Returns pointer to a hole if the memory space resides inside one.
 * Ext is added to the compared hole size. It allows accesses to the last
 * byte of the hole to not count as outside the hole.
*/
dsm_hole *dsm_in_hole (off_t offset, size_t size, size_t ext, 
	dsm_hole *holes) {
	dsm_hole *hole;
	off_t start = offset, end = offset + size;
	off_t hole_start, hole_end;

	for (hole = holes; hole != NULL; hole = hole->next) {
		hole_start = hole->offset;
		hole_end = hole_start + hole->size + ext;

		// If start begins in that hole, ensure end is before hole end.
		if (start >= hole_start && start < hole_end) {
			return ((end <= hole_end) ? hole : NULL);
		}
	}

	return NULL;
}

// Returns a pointer to a hole given a hole identifier. Returns NULL on error.
dsm_hole *dsm_get_hole (int id, dsm_hole *holes) {
	
	// Base case: No hole.
	if (holes == NULL) {
		return NULL;
	}

	// Base case: At hole.
	if (holes->id == id) {
		return holes;
	}

	// Recursive case: Search next hole.
	return dsm_get_hole(id, holes->next);
}

// Returns nonzero if given memory range overlaps with a hole.
int dsm_overlaps_hole (off_t offset, size_t size, dsm_hole *holes) {
	dsm_hole *hole;
	unsigned int start = offset, end = offset + size;
	unsigned int hole_start, hole_end;

	for (hole = holes; hole != NULL; hole = hole->next) {
		hole_start = hole->offset;
		hole_end = hole_start + hole->size;

		// Case: Start or End is inside another hole.
		if ((start >= hole_start && start < hole_end) ||
			(end > hole_start && end < hole_end)) {
			break;
		}

		// Case: A hole is within the given range.
		if (start < hole_start && end >= hole_end) {
			break;
		}
	}

	return (hole != NULL);
}

// Removes a hole. Returns nonzero on error.
int dsm_del_hole (int id, dsm_hole **holes) {
	int exists = 0;

	// Remove the hole, if it exists.
	*holes = remove_hole(id, &exists, *holes);

	return (exists != 1);
}

// [DEBUG] Prints all holes to output.
void dsm_show_holes (dsm_hole *holes) {

	// Base case.
	if (holes == NULL) {
		return;
	}

	printf("[%d]: %ld -> %lu\n", holes->id, holes->offset,
		holes->offset + holes->size);

	dsm_show_holes(holes->next);
}