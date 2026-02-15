#ifndef COUGFS_H
#define COUGFS_H
typedef struct {
    int magic;
    int block_size;
    int inode_count;
} superblock_t;
typedef struct {
    int type;
    int size;
} inode_t;
#endif
