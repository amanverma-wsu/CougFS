/*
 * CougFS: Filesystem Mount/Format Operations
 * WSU CPTS 360
 *
 * Implements fs_format(), fs_mount(), fs_unmount(), and superblock helpers.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "fs.h"
#include "disk.h"
#include "bitmap.h"
#include "inode.h"

/* In-memory copy of the superblock, loaded on mount */
static cougfs_superblock_t g_superblock;
static int g_mounted = 0;

/* ------------------------------------------------------------------ */
/* Internal helpers                                                     */
/* ------------------------------------------------------------------ */

/*
 * Write the root directory's two mandatory entries ("." and "..") into
 * the first data block of the root inode.
 * Both point to ROOT_INODE (0) since root is its own parent.
 */
static int write_root_dir_block(uint32_t block_num)
{
    uint8_t buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);

    cougfs_dir_entry_t *entries = (cougfs_dir_entry_t *)buf;

    /* Entry 0: "." — current directory */
    entries[0].inode     = ROOT_INODE;
    entries[0].file_type = DT_DIR;
    entries[0].name_len  = 1;
    strncpy(entries[0].name, ".", MAX_NAME_LEN);

    /* Entry 1: ".." — parent directory (same as root for /) */
    entries[1].inode     = ROOT_INODE;
    entries[1].file_type = DT_DIR;
    entries[1].name_len  = 2;
    strncpy(entries[1].name, "..", MAX_NAME_LEN);

    return disk_write_block(block_num, buf);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int fs_format(const char *path)
{
    /* Step 1: open (or create) the disk image */
    if (disk_open(path) < 0) {
        fprintf(stderr, "fs_format: failed to open disk image\n");
        return -1;
    }

    /* Step 2: zero out every metadata block so there are no stale bits */
    uint8_t zero[BLOCK_SIZE];
    memset(zero, 0, BLOCK_SIZE);

    /* Superblock + bitmaps + inode table + journal */
    uint32_t meta_end = DATA_BLOCK_START;
    for (uint32_t b = 0; b < meta_end; b++) {
        if (disk_write_block(b, zero) < 0) {
            fprintf(stderr, "fs_format: failed to zero block %u\n", b);
            disk_close();
            return -1;
        }
    }

    /* Step 3: initialize the superblock */
    memset(&g_superblock, 0, sizeof(g_superblock));
    g_superblock.magic            = COUGFS_MAGIC;
    g_superblock.version          = 1;
    g_superblock.block_size       = BLOCK_SIZE;
    g_superblock.total_blocks     = DISK_SIZE_BLOCKS;
    g_superblock.total_inodes     = MAX_INODES;
    g_superblock.free_inodes      = MAX_INODES - 1;  /* root inode consumed */
    g_superblock.free_blocks      = MAX_DATA_BLOCKS - 1; /* root data block consumed */
    g_superblock.data_block_start = DATA_BLOCK_START;
    g_superblock.inode_table_start= INODE_TABLE_START;
    g_superblock.journal_start    = JOURNAL_START;
    g_superblock.journal_blocks   = JOURNAL_BLOCKS;
    g_superblock.mount_count      = 0;
    g_superblock.state            = FS_STATE_CLEAN;

    if (disk_write_block(SUPERBLOCK_BLOCK, &g_superblock) < 0) {
        fprintf(stderr, "fs_format: failed to write superblock\n");
        disk_close();
        return -1;
    }

    /* Step 4: initialize in-memory bitmaps from the (zeroed) disk */
    if (bitmap_init() < 0) {
        fprintf(stderr, "fs_format: bitmap_init failed\n");
        disk_close();
        return -1;
    }

    /* Step 5: allocate root inode (must be inode 0) */
    int root_ino = bitmap_alloc_inode();
    if (root_ino != ROOT_INODE) {
        fprintf(stderr, "fs_format: root inode allocation failed (got %d)\n", root_ino);
        disk_close();
        return -1;
    }

    /* Step 6: allocate first data block for root directory contents */
    int root_block = bitmap_alloc_block();
    if (root_block < 0) {
        fprintf(stderr, "fs_format: root data block allocation failed\n");
        disk_close();
        return -1;
    }

    /* Step 7: write "." and ".." into the root directory block */
    if (write_root_dir_block((uint32_t)root_block) < 0) {
        fprintf(stderr, "fs_format: failed to write root dir entries\n");
        disk_close();
        return -1;
    }

    /* Step 8: build and write the root inode */
    cougfs_inode_t root_inode;
    memset(&root_inode, 0, sizeof(root_inode));
    root_inode.mode       = COUGFS_S_IFDIR | COUGFS_S_IRWXU;
    root_inode.uid        = 0;
    root_inode.gid        = 0;
    root_inode.link_count = 2;   /* "." inside itself + parent ref */
    root_inode.size       = 2 * DIR_ENTRY_SIZE;
    root_inode.atime      = (uint32_t)time(NULL);
    root_inode.mtime      = root_inode.atime;
    root_inode.ctime      = root_inode.atime;
    root_inode.blocks     = 1;
    root_inode.direct[0]  = (uint32_t)root_block;
    root_inode.indirect   = INVALID_BLOCK;

    if (inode_write(ROOT_INODE, &root_inode) < 0) {
        fprintf(stderr, "fs_format: failed to write root inode\n");
        disk_close();
        return -1;
    }

    /* Step 9: flush bitmaps to disk */
    if (bitmap_sync() < 0) {
        fprintf(stderr, "fs_format: bitmap_sync failed\n");
        disk_close();
        return -1;
    }

    disk_sync();
    disk_close();

    printf("fs_format: CougFS formatted successfully on '%s'\n", path);
    printf("  Total blocks : %u\n", DISK_SIZE_BLOCKS);
    printf("  Total inodes : %u\n", MAX_INODES);
    printf("  Data start   : block %u\n", DATA_BLOCK_START);
    return 0;
}

int fs_mount(const char *path)
{
    if (g_mounted) {
        fprintf(stderr, "fs_mount: filesystem already mounted\n");
        return -1;
    }

    /* Step 1: open the disk image */
    if (disk_open(path) < 0) {
        fprintf(stderr, "fs_mount: failed to open '%s'\n", path);
        return -1;
    }

    /* Step 2: read and validate the superblock */
    if (disk_read_block(SUPERBLOCK_BLOCK, &g_superblock) < 0) {
        fprintf(stderr, "fs_mount: failed to read superblock\n");
        disk_close();
        return -1;
    }

    if (g_superblock.magic != COUGFS_MAGIC) {
        fprintf(stderr, "fs_mount: bad magic (got 0x%08X, expected 0x%08X)\n",
                g_superblock.magic, COUGFS_MAGIC);
        disk_close();
        return -1;
    }

    if (g_superblock.state == FS_STATE_DIRTY) {
        fprintf(stderr, "fs_mount: WARNING — filesystem was not cleanly unmounted\n");
    }

    /* Step 3: load bitmaps into memory */
    if (bitmap_init() < 0) {
        fprintf(stderr, "fs_mount: bitmap_init failed\n");
        disk_close();
        return -1;
    }

    /* Step 4: mark filesystem dirty and update mount count */
    g_superblock.state = FS_STATE_DIRTY;
    g_superblock.mount_count++;

    if (fs_sync_superblock() < 0) {
        fprintf(stderr, "fs_mount: failed to update superblock\n");
        disk_close();
        return -1;
    }

    g_mounted = 1;
    printf("fs_mount: mounted '%s' (mount #%u)\n", path, g_superblock.mount_count);
    return 0;
}

void fs_unmount(void)
{
    if (!g_mounted) {
        fprintf(stderr, "fs_unmount: no filesystem mounted\n");
        return;
    }

    /* Flush bitmaps */
    bitmap_sync();

    /* Mark clean and write superblock */
    g_superblock.state       = FS_STATE_CLEAN;
    g_superblock.free_inodes = bitmap_free_inode_count();
    g_superblock.free_blocks = bitmap_free_block_count();
    fs_sync_superblock();

    disk_sync();
    disk_close();

    g_mounted = 0;
    printf("fs_unmount: filesystem unmounted cleanly\n");
}

cougfs_superblock_t *fs_get_superblock(void)
{
    return &g_superblock;
}

int fs_sync_superblock(void)
{
    /* Keep free counts accurate before every write */
    g_superblock.free_inodes = bitmap_free_inode_count();
    g_superblock.free_blocks = bitmap_free_block_count();

    if (disk_write_block(SUPERBLOCK_BLOCK, &g_superblock) < 0) {
        fprintf(stderr, "fs_sync_superblock: write failed\n");
        return -1;
    }
    return 0;
}