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
    if (inode_write(ino, &inode) < 0)
        return -1;

    bitmap_free_inode(ino);
    bitmap_sync();

    return 0;
}

static uint32_t handle_direct_block(cougfs_inode_t *inode, uint32_t logical_block, int alloc)
{
    if (inode->direct[logical_block] != INVALID_BLOCK)
        return inode->direct[logical_block];

    if (!alloc)
        return INVALID_BLOCK;

    int blk = bitmap_alloc_block();

    if (blk < 0)
        return INVALID_BLOCK;

    if (disk_zero_and_write_block((uint32_t)blk) < 0) {
        bitmap_free_block((uint32_t)blk);
        return INVALID_BLOCK;
    }

    inode->direct[logical_block] = (uint32_t)blk;
    inode->blocks++;

    return (uint32_t)blk;
}

static uint32_t handle_indirect_block(cougfs_inode_t *inode,
                                      uint32_t logical_block,
                                      int alloc)
{
    uint32_t indirect_idx = logical_block - DIRECT_BLOCKS;

    if (indirect_idx >= BLOCK_SIZE / sizeof(uint32_t))
        return INVALID_BLOCK;

    if (inode->indirect == INVALID_BLOCK) {
        if (!alloc)
            return INVALID_BLOCK;

        int iblk = bitmap_alloc_block();

        if (iblk < 0)
            return INVALID_BLOCK;

        if (disk_zero_and_write_block((uint32_t)iblk) < 0) {
            bitmap_free_block((uint32_t)iblk);
            return INVALID_BLOCK;
        }

        inode->indirect = (uint32_t)iblk;
    }

    uint32_t indirect_buf[BLOCK_SIZE / sizeof(uint32_t)];

    if (disk_read_block(inode->indirect, indirect_buf) < 0)
        return INVALID_BLOCK;

    if (indirect_buf[indirect_idx] != INVALID_BLOCK)
        return indirect_buf[indirect_idx];

    if (!alloc)
        return INVALID_BLOCK;

    int dblk = bitmap_alloc_block();

    if (dblk < 0)
        return INVALID_BLOCK;

    if (disk_zero_and_write_block((uint32_t)dblk) < 0) {
        bitmap_free_block((uint32_t)dblk);
        return INVALID_BLOCK;
    }

    indirect_buf[indirect_idx] = (uint32_t)dblk;

    if (disk_write_block(inode->indirect, indirect_buf) < 0) {
        bitmap_free_block((uint32_t)dblk);
        indirect_buf[indirect_idx] = INVALID_BLOCK;
        return INVALID_BLOCK;
    }

    inode->blocks++;
    return (uint32_t)dblk;
}

uint32_t inode_get_block(cougfs_inode_t *inode, uint32_t logical_block, int alloc)
{
    if (logical_block < DIRECT_BLOCKS)
        return handle_direct_block(inode, logical_block, alloc);

    return handle_indirect_block(inode, logical_block, alloc);
}

int inode_truncate(uint32_t ino, cougfs_inode_t *inode, uint32_t new_size)
{
    uint32_t max_size = MAX_FILE_BLOCKS * BLOCK_SIZE;

    if (new_size > max_size)
        return -1;

    uint32_t old_blocks = (inode->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t new_blocks = (new_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    if (new_size == 0)
        new_blocks = 0;

    uint32_t indirect_buf[BLOCK_SIZE / sizeof(uint32_t)];
    int indirect_loaded = 0;
    int indirect_dirty = 0;

    if (inode->indirect != INVALID_BLOCK && old_blocks > DIRECT_BLOCKS) {
        if (disk_read_block(inode->indirect, indirect_buf) < 0)
            return -1;

        indirect_loaded = 1;
    }

    for (uint32_t i = new_blocks; i < old_blocks; i++) {
        uint32_t blk = inode_get_block(inode, i, 0);

        if (blk != INVALID_BLOCK) {
            bitmap_free_block(blk);

            if (i < DIRECT_BLOCKS) {
                inode->direct[i] = INVALID_BLOCK;
            } else if (indirect_loaded) {
                indirect_buf[i - DIRECT_BLOCKS] = INVALID_BLOCK;
                indirect_dirty = 1;
            }

            if (inode->blocks > 0)
                inode->blocks--;
        }
    }

    if (new_blocks <= DIRECT_BLOCKS && inode->indirect != INVALID_BLOCK) {
        uint32_t zero_buf[BLOCK_SIZE / sizeof(uint32_t)];
        memset(zero_buf, 0, sizeof(zero_buf));

        if (disk_write_block(inode->indirect, zero_buf) < 0)
            return -1;

        bitmap_free_block(inode->indirect);
        inode->indirect = INVALID_BLOCK;
        indirect_dirty = 0;
    } else if (indirect_dirty) {
        if (disk_write_block(inode->indirect, indirect_buf) < 0)
            return -1;
    }

    inode->size = new_size;
    inode->mtime = (uint32_t)time(NULL);

    if (inode_write(ino, inode) < 0)
        return -1;

    bitmap_sync();
    return 0;
}