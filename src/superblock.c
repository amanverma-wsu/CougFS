#include <string.h>
#include <stdio.h>
#include "superblock.h"
#include "disk.h"

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
    (void)sb;
    return -1;
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
