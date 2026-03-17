/*
 * CougFS: A User-Space Unix-Like File System
 * WSU CPTS 360
 *
 * Core filesystem data structures and constants.
 */

#ifndef COUGFS_H
#define COUGFS_H

#include <stdint.h>
#include <time.h>

/* Disk layout constants */
#define BLOCK_SIZE         4096
#define DISK_SIZE_BLOCKS   4096        /* 16 MB total */
#define DISK_SIZE          (BLOCK_SIZE * DISK_SIZE_BLOCKS)

/* On-disk layout (block numbers) */
#define SUPERBLOCK_BLOCK   0
#define INODE_BITMAP_BLOCK 1
#define DATA_BITMAP_BLOCK  2
#define INODE_TABLE_START  3
#define INODE_TABLE_BLOCKS 8           /* 8 blocks for inode table */
#define JOURNAL_START      11
#define JOURNAL_BLOCKS     8           /* 8 blocks for journal */
#define DATA_BLOCK_START   19          /* first usable data block */

/* Inode constants */
#define INODE_SIZE         128
#define INODES_PER_BLOCK   (BLOCK_SIZE / INODE_SIZE)  /* 32 */
#define MAX_INODES         (INODE_TABLE_BLOCKS * INODES_PER_BLOCK) /* 256 */
#define DIRECT_BLOCKS      12
#define MAX_DATA_BLOCKS    (DISK_SIZE_BLOCKS - DATA_BLOCK_START) /* ~4077 */

/* Directory entry constants */
#define DIR_ENTRY_SIZE     256
#define DIR_ENTRIES_PER_BLOCK (BLOCK_SIZE / DIR_ENTRY_SIZE) /* 16 */
#define MAX_NAME_LEN       251

/* File type bits stored in mode */
#define COUGFS_S_IFREG     0x8000     /* regular file */
#define COUGFS_S_IFDIR     0x4000     /* directory */
#define COUGFS_S_IFMT      0xF000     /* file type mask */

/* Permission bits */
#define COUGFS_S_IRWXU     0700
#define COUGFS_S_IRUSR     0400
#define COUGFS_S_IWUSR     0200
#define COUGFS_S_IXUSR     0100
#define COUGFS_S_IRWXG     0070
#define COUGFS_S_IRWXO     0007

/* Special inode numbers */
#define ROOT_INODE         0
#define INVALID_INODE      0xFFFFFFFF
#define INVALID_BLOCK      0

/* Magic number for filesystem identification */
#define COUGFS_MAGIC       0x434F5547  /* "COUG" */

/* Maximum file size: 12 direct + 1024 indirect = 1036 blocks */
#define MAX_FILE_BLOCKS    (DIRECT_BLOCKS + (BLOCK_SIZE / sizeof(uint32_t)))

/*
 * Superblock: stored in block 0.
 * Tracks global filesystem metadata.
 */
typedef struct {
    uint32_t magic;               /* COUGFS_MAGIC */
    uint32_t version;             /* filesystem version */
    uint32_t block_size;          /* block size in bytes */
    uint32_t total_blocks;        /* total number of blocks */
    uint32_t total_inodes;        /* total number of inodes */
    uint32_t free_blocks;         /* number of free data blocks */
    uint32_t free_inodes;         /* number of free inodes */
    uint32_t data_block_start;    /* first data block number */
    uint32_t inode_table_start;   /* first inode table block */
    uint32_t journal_start;       /* first journal block */
    uint32_t journal_blocks;      /* number of journal blocks */
    uint32_t mount_count;         /* number of times mounted */
    uint32_t state;               /* 0 = clean, 1 = dirty */
    uint8_t  padding[BLOCK_SIZE - 52];
} cougfs_superblock_t;

/*
 * Inode: 128 bytes.
 * Stores file/directory metadata and block pointers.
 */
typedef struct {
    uint16_t mode;                /* file type and permissions */
    uint16_t uid;                 /* owner user ID */
    uint16_t gid;                 /* owner group ID */
    uint16_t link_count;          /* hard link count */
    uint32_t size;                /* file size in bytes */
    uint32_t atime;               /* last access time */
    uint32_t mtime;               /* last modification time */
    uint32_t ctime;               /* creation time */
    uint32_t blocks;              /* number of allocated blocks */
    uint32_t direct[DIRECT_BLOCKS]; /* direct block pointers */
    uint32_t indirect;            /* single indirect block pointer */
    uint8_t  padding[8];          /* pad to 128 bytes */
} cougfs_inode_t;

/*
 * Directory entry: 256 bytes (fixed size).
 * Maps a name to an inode number.
 */
typedef struct {
    uint32_t inode;               /* inode number (INVALID_INODE = unused) */
    uint8_t  file_type;           /* COUGFS_S_IFREG or COUGFS_S_IFDIR >> 12 */
    uint8_t  name_len;            /* length of name */
    char     name[MAX_NAME_LEN + 1]; /* null-terminated file name */
} cougfs_dir_entry_t;

/* File type values for dir_entry.file_type */
#define DT_REG  1
#define DT_DIR  2

/* Filesystem state */
#define FS_STATE_CLEAN  0
#define FS_STATE_DIRTY  1

#endif /* COUGFS_H */
