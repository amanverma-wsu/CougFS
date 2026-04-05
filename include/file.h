/*
 * CougFS: File Operations
 *
 * Create, read, write, delete, and truncate files.
 */

#ifndef FILE_H
#define FILE_H

#include "cougfs.h"

/* File descriptor table entry */
typedef struct {
    int      in_use;         /* 1 if this fd is open */
    uint32_t inode;          /* inode number */
    uint32_t offset;         /* current read/write offset */
    int      flags;          /* open flags */
} cougfs_fd_t;

#define MAX_OPEN_FILES 64

/* Open flags */
#define COUGFS_O_RDONLY  0x01
#define COUGFS_O_WRONLY  0x02
#define COUGFS_O_RDWR    0x03
#define COUGFS_O_CREAT   0x10
#define COUGFS_O_TRUNC   0x20

/* Initialize the file descriptor table. */
void file_init(void);

/* Create a new file in a directory. Returns the file's inode or -1. */
int file_create(uint32_t dir_ino, const char *name, uint16_t mode);

/* Open a file. Returns file descriptor index or -1. */
int file_open(uint32_t ino, int flags);

/* Read up to count bytes from fd into buf. Returns bytes read or -1. */
int file_read(int fd, void *buf, uint32_t count);

/* Write count bytes from buf to fd. Returns bytes written or -1. */
int file_write(int fd, const void *buf, uint32_t count);

/* Seek to offset in fd. whence: 0=SET, 1=CUR, 2=END. Returns new offset or -1. */
int file_seek(int fd, int32_t offset, int whence);

/* Close a file descriptor. Returns 0 on success. */
int file_close(int fd);

/* Delete (unlink) a file from a directory. Returns 0 on success. */
int file_delete(uint32_t dir_ino, const char *name);

/* Truncate a file to the given size. Returns 0 on success. */
int file_truncate(uint32_t ino, uint32_t new_size);

/* Get the stat info for a file. Returns 0 on success. */
int file_stat(uint32_t ino, cougfs_inode_t *out);

#endif /* FILE_H */
