/*
 * CougFS: Concurrency Layer
 *
 * Thread-safe wrappers using POSIX reader/writer locks.
 * Provides per-inode locking and a global filesystem lock.
 */

#ifndef CONCURRENCY_H
#define CONCURRENCY_H

#include <pthread.h>
#include "cougfs.h"

/* Initialize the concurrency subsystem. */
int concurrency_init(void);

/* Destroy the concurrency subsystem. */
void concurrency_destroy(void);

/* Acquire a read lock on the global filesystem. */
void fs_read_lock(void);

/* Release a read lock on the global filesystem. */
void fs_read_unlock(void);

/* Acquire a write lock on the global filesystem. */
void fs_write_lock(void);

/* Release a write lock on the global filesystem. */
void fs_write_unlock(void);

/* Acquire a read lock on a specific inode. */
void inode_read_lock(uint32_t ino);

/* Release a read lock on a specific inode. */
void inode_read_unlock(uint32_t ino);

/* Acquire a write lock on a specific inode. */
void inode_write_lock(uint32_t ino);

/* Release a write lock on a specific inode. */
void inode_write_unlock(uint32_t ino);

#endif /* CONCURRENCY_H */
