#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include "cougfs.h"

/* Read the superblock from block 0. Returns 0 on success, -1 on invalid data. */
int superblock_load(cougfs_superblock_t *sb);

/* Format a fresh filesystem image and initialize the superblock. */
int superblock_format(cougfs_superblock_t *sb);

/* Persist the current superblock to block 0. */
int superblock_sync(const cougfs_superblock_t *sb);

#endif
