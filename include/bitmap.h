/*
 * CougFS: Bitmap Operations
 *
 * Manages inode and data block allocation bitmaps.
 */

#ifndef BITMAP_H
#define BITMAP_H

#include "cougfs.h"

/* Initialize bitmaps from disk. Returns 0 on success. */
int bitmap_init(void);

/* Flush bitmaps back to disk. Returns 0 on success. */
int bitmap_sync(void);

/* Allocate a free inode. Returns inode number or -1 if full. */
int bitmap_alloc_inode(void);

/* Free an inode. */
void bitmap_free_inode(uint32_t ino);

/* Check if an inode is allocated. Returns 1 if allocated, 0 if free. */
int bitmap_inode_is_set(uint32_t ino);

/* Allocate a free data block. Returns block number or -1 if full. */
int bitmap_alloc_block(void);

/* Free a data block. */
void bitmap_free_block(uint32_t block_num);

/* Check if a data block is allocated. Returns 1 if allocated, 0 if free. */
int bitmap_block_is_set(uint32_t block_num);

/* Return count of free inodes. */
uint32_t bitmap_free_inode_count(void);

/* Return count of free data blocks. */
uint32_t bitmap_free_block_count(void);

#endif /* BITMAP_H */
