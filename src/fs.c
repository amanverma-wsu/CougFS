#include <stdio.h>
#include <string.h>
#include <time.h>
#include "fs.h"
#include "disk.h"
#include "bitmap.h"
#include "inode.h"
#include "dir.h"
#include "file.h"
#include "journal.h"
#include "concurrency.h"

static cougfs_superblock_t superblock;
static int mounted = 0;

int fs_format(const char *disk_path)
{
    if (disk_open(disk_path) < 0)
        return -1;
    memset(&superblock, 0, sizeof(superblock));
    superblock.magic = COUGFS_MAGIC;
    superblock.version = 1;
    superblock.block_size = BLOCK_SIZE;
    superblock.total_blocks = DISK_SIZE_BLOCKS;
    superblock.total_inodes = MAX_INODES;
    superblock.free_blocks = MAX_DATA_BLOCKS;
    superblock.free_inodes = MAX_INODES;
    superblock.data_block_start = DATA_BLOCK_START;
    superblock.inode_table_start = INODE_TABLE_START;
    superblock.journal_start = JOURNAL_START;
    superblock.journal_blocks = JOURNAL_BLOCKS;
    superblock.mount_count = 0;
    superblock.state = FS_STATE_CLEAN;
    if (disk_write_block(SUPERBLOCK_BLOCK, &superblock) < 0) {
        disk_close();
        return -1;
    }
    uint8_t zeros[BLOCK_SIZE];
    memset(zeros, 0, BLOCK_SIZE);
    disk_write_block(INODE_BITMAP_BLOCK, zeros);
    disk_write_block(DATA_BITMAP_BLOCK, zeros);
    for (int i = 0; i < INODE_TABLE_BLOCKS; i++) {
        disk_write_block(INODE_TABLE_START + i, zeros);
    }
    for (int i = 0; i < JOURNAL_BLOCKS; i++) {
        disk_write_block(JOURNAL_START + i, zeros);
    }
    bitmap_init();
    int root_ino = inode_alloc(COUGFS_S_IFDIR | 0755);
    if (root_ino != ROOT_INODE) {
        fprintf(stderr, "fs_format: root inode is %d, expected %d\n",
                root_ino, ROOT_INODE);
        disk_close();
        return -1;
    }
    dir_add_entry(ROOT_INODE, ".", ROOT_INODE, DT_DIR);
    dir_add_entry(ROOT_INODE, "..", ROOT_INODE, DT_DIR);
    superblock.free_inodes = bitmap_free_inode_count();
    superblock.free_blocks = bitmap_free_block_count();
    disk_write_block(SUPERBLOCK_BLOCK, &superblock);
    disk_sync();
    disk_close();
    printf("CougFS: formatted %s (%u blocks, %u inodes)\n",
           disk_path, DISK_SIZE_BLOCKS, MAX_INODES);
    return 0;
}

int fs_mount(const char *disk_path)
{
    if (mounted) {
        fprintf(stderr, "fs_mount: already mounted\n");
        return -1;
    }
    if (disk_open(disk_path) < 0)
        return -1;
    if (disk_read_block(SUPERBLOCK_BLOCK, &superblock) < 0) {
        disk_close();
        return -1;
    }
    if (superblock.magic != COUGFS_MAGIC) {
        fprintf(stderr, "fs_mount: invalid magic 0x%08x (expected 0x%08x)\n",
                superblock.magic, COUGFS_MAGIC);
        disk_close();
        return -1;
    }
    if (bitmap_init() < 0) {
        disk_close();
        return -1;
    }
    concurrency_init();
    file_init();
    journal_init();
    if (superblock.state == FS_STATE_DIRTY) {
        printf("CougFS: filesystem was not cleanly unmounted, recovering...\n");
        journal_recover();
    }
    superblock.mount_count++;
    superblock.state = FS_STATE_DIRTY;
    disk_write_block(SUPERBLOCK_BLOCK, &superblock);
    disk_sync();
    mounted = 1;
    printf("CougFS: mounted %s (v%u, %u blocks, %u inodes, %u free blocks)\n",
           disk_path, superblock.version, superblock.total_blocks,
           superblock.total_inodes, superblock.free_blocks);
    return 0;
}

void fs_unmount(void)
{
    if (!mounted)
        return;
    bitmap_sync();
    journal_sync();
    superblock.free_inodes = bitmap_free_inode_count();
    superblock.free_blocks = bitmap_free_block_count();
    superblock.state = FS_STATE_CLEAN;
    disk_write_block(SUPERBLOCK_BLOCK, &superblock);
    disk_sync();
    concurrency_destroy();
    disk_close();
    mounted = 0;
    printf("CougFS: unmounted\n");
}

const cougfs_superblock_t *fs_get_superblock(void)
{
    return &superblock;
}

int fs_sync_superblock(void)
{
    superblock.free_inodes = bitmap_free_inode_count();
    superblock.free_blocks = bitmap_free_block_count();
    return disk_write_block(SUPERBLOCK_BLOCK, &superblock);
}