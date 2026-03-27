/*
 * CougFS: Directory Operations
 *
 * Create, list, and navigate directories.
 */

#ifndef DIR_H
#define DIR_H

#include "cougfs.h"

/* Look up a name in a directory inode. Returns inode number or -1 if not found. */
int dir_lookup(uint32_t dir_ino, const char *name);

/* Add an entry to a directory. Returns 0 on success. */
int dir_add_entry(uint32_t dir_ino, const char *name, uint32_t child_ino, uint8_t file_type);

/* Remove an entry from a directory. Returns 0 on success. */
int dir_remove_entry(uint32_t dir_ino, const char *name);

/* List all entries in a directory. Calls callback for each entry.
 * Callback receives: name, inode number, file type. */
int dir_list(uint32_t dir_ino,
             void (*callback)(const char *name, uint32_t ino, uint8_t file_type, void *ctx),
             void *ctx);

/* Check if a directory is empty (only . and .. entries). Returns 1 if empty. */
int dir_is_empty(uint32_t dir_ino);

/* Create a new directory under parent_ino with given name. Returns new inode or -1. */
int dir_create(uint32_t parent_ino, const char *name, uint16_t mode);

/* Remove a directory (must be empty). Returns 0 on success. */
int dir_remove(uint32_t parent_ino, const char *name);

/* Resolve a path (absolute, starting from root) to an inode number.
 * Returns inode number or -1 if not found. */
int dir_resolve_path(const char *path);

/* Resolve a path relative to cwd_ino. Returns inode number or -1. */
int dir_resolve_path_from(uint32_t cwd_ino, const char *path);

#endif /* DIR_H */
