#include <string.h>
#include <stdio.h>
#include "superblock.h"
#include "disk.h"
#include "bitmap.h"

int superblock_load(cougfs_superblock_t *sb)
{
    uint8_t buf[BLOCK_SIZE];

    if (disk_read_block(SUPERBLOCK_BLOCK, buf) < 0)
        return -1;

    memcpy(sb, buf, sizeof(cougfs_superblock_t));

    if (sb->magic != COUGFS_MAGIC)
        return -1;
    if (sb->block_size != BLOCK_SIZE)
        return -1;
    if (sb->total_blocks != DISK_SIZE_BLOCKS)
        return -1;

    return 0;
}

int superblock_format(cougfs_superblock_t *sb)
{
    uint8_t buf[BLOCK_SIZE];
    uint8_t zero[BLOCK_SIZE];

    memset(buf, 0, sizeof(buf));
    memset(zero, 0, sizeof(zero));
    memset(sb, 0, sizeof(*sb));

    sb->magic             = COUGFS_MAGIC;
    sb->version           = 1;
    sb->block_size        = BLOCK_SIZE;
    sb->total_blocks      = DISK_SIZE_BLOCKS;
    sb->total_inodes      = MAX_INODES;
    sb->free_blocks       = MAX_DATA_BLOCKS;
    sb->free_inodes       = MAX_INODES;
    sb->data_block_start  = DATA_BLOCK_START;
    sb->inode_table_start = INODE_TABLE_START;
    sb->journal_start     = JOURNAL_START;
    sb->journal_blocks    = JOURNAL_BLOCKS;
    sb->mount_count       = 0;
    sb->state             = FS_STATE_CLEAN;

    memcpy(buf, sb, sizeof(cougfs_superblock_t));
    if (disk_write_block(SUPERBLOCK_BLOCK, buf) < 0)
        return -1;

    /* Bitmap inode & data = all free. */
    if (disk_write_block(INODE_BITMAP_BLOCK, zero) < 0)
        return -1;
    if (disk_write_block(DATA_BITMAP_BLOCK, zero) < 0)
        return -1;

    /* Inode table = zero. */
    for (uint32_t b = 0; b < INODE_TABLE_BLOCKS; b++) {
        if (disk_write_block(INODE_TABLE_START + b, zero) < 0)
            return -1;
    }

    return 0;
}

int superblock_sync(const cougfs_superblock_t *sb)
{
    uint8_t buf[BLOCK_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, sb, sizeof(cougfs_superblock_t));
    if (disk_write_block(SUPERBLOCK_BLOCK, buf) < 0)
        return -1;
    return 0;
}
