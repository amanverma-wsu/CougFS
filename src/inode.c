#include <stdio.h>
#include <string.h>
#include <time.h>
#include "inode.h"
#include "disk.h"
#include "bitmap.h"

int inode_read(uint32_t ino, cougfs_inode_t *inode)
{
    if (ino >= MAX_INODES) {
        fprintf(stderr, "inode_read: inode %u out of range\n", ino);
        return -1;
    }
    uint32_t block = INODE_TABLE_START + (ino / INODES_PER_BLOCK);
    uint32_t offset_in_block = (ino % INODES_PER_BLOCK) * INODE_SIZE;
    uint8_t buf[BLOCK_SIZE];
    if (disk_read_block(block, buf) < 0)
        return -1;
    memcpy(inode, buf + offset_in_block, sizeof(cougfs_inode_t));
    return 0;
}

int inode_write(uint32_t ino, const cougfs_inode_t *inode)
{
    if (ino >= MAX_INODES) {
        fprintf(stderr, "inode_write: inode %u out of range\n", ino);
        return -1;
    }
    uint32_t block = INODE_TABLE_START + (ino / INODES_PER_BLOCK);
    uint32_t offset_in_block = (ino % INODES_PER_BLOCK) * INODE_SIZE;
    uint8_t buf[BLOCK_SIZE];
    if (disk_read_block(block, buf) < 0)
        return -1;
    memcpy(buf + offset_in_block, inode, sizeof(cougfs_inode_t));
    if (disk_write_block(block, buf) < 0)
        return -1;
    return 0;
}

int inode_alloc(uint16_t mode)
{
    int ino = bitmap_alloc_inode();
    if (ino < 0)
        return -1;
    cougfs_inode_t inode;
    memset(&inode, 0, sizeof(inode));
    inode.mode = mode;
    inode.uid = 0;
    inode.gid = 0;
    inode.link_count = 1;
    inode.size = 0;
    inode.atime = (uint32_t)time(NULL);
    inode.mtime = inode.atime;
    inode.ctime = inode.atime;
    inode.blocks = 0;
    if (inode_write(ino, &inode) < 0) {
        bitmap_free_inode(ino);
        return -1;
    }
    bitmap_sync();
    return ino;
}

int inode_free(uint32_t ino)
{
    cougfs_inode_t inode;
    if (inode_read(ino, &inode) < 0)
        return -1;
    for (int i = 0; i < DIRECT_BLOCKS; i++) {
        if (inode.direct[i] != INVALID_BLOCK) {
            bitmap_free_block(inode.direct[i]);
            inode.direct[i] = INVALID_BLOCK;
        }
    }
    if (inode.indirect != INVALID_BLOCK) {
        uint32_t indirect_buf[BLOCK_SIZE / sizeof(uint32_t)];
        if (disk_read_block(inode.indirect, indirect_buf) == 0) {
            for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t); i++) {
                if (indirect_buf[i] != INVALID_BLOCK)
                    bitmap_free_block(indirect_buf[i]);
            }
        }
        bitmap_free_block(inode.indirect);
        inode.indirect = INVALID_BLOCK;
    }
    memset(&inode, 0, sizeof(inode));
    inode_write(ino, &inode);
    bitmap_free_inode(ino);
    bitmap_sync();
    return 0;
}

uint32_t inode_get_block(cougfs_inode_t *inode, uint32_t logical_block, int alloc)
{
    /* TODO: implement direct and indirect block mapping */
    (void)inode;
    (void)logical_block;
    (void)alloc;
    return INVALID_BLOCK;
}

int inode_truncate(uint32_t ino, cougfs_inode_t *inode, uint32_t new_size)
{
    /* TODO: implement file truncation */
    (void)ino;
    (void)inode;
    (void)new_size;
    return -1;
}