/*
 * CougFS - Entry Point
 * WSU CPTS 360
 */

#include <stdio.h>
#include "cougfs.h"
#include "fs.h"

#define DISK_IMAGE "disk.img"

int main(void)
{
    printf("=== CougFS ===\n\n");

    /* Format a fresh filesystem */
    printf("-- Formatting disk image --\n");
    if (fs_format(DISK_IMAGE) < 0) {
        fprintf(stderr, "Format failed\n");
        return 1;
    }

    /* Mount it */
    printf("\n-- Mounting --\n");
    if (fs_mount(DISK_IMAGE) < 0) {
        fprintf(stderr, "Mount failed\n");
        return 1;
    }

    /* Print superblock info */
    cougfs_superblock_t *sb = fs_get_superblock();
    printf("\n-- Superblock --\n");
    printf("  Magic        : 0x%08X\n", sb->magic);
    printf("  Version      : %u\n",     sb->version);
    printf("  Block size   : %u bytes\n", sb->block_size);
    printf("  Total blocks : %u\n",     sb->total_blocks);
    printf("  Total inodes : %u\n",     sb->total_inodes);
    printf("  Free blocks  : %u\n",     sb->free_blocks);
    printf("  Free inodes  : %u\n",     sb->free_inodes);
    printf("  Mount count  : %u\n",     sb->mount_count);
    printf("  State        : %s\n",     sb->state == FS_STATE_DIRTY ? "dirty" : "clean");

    /* Cleanly unmount */
    printf("\n-- Unmounting --\n");
    fs_unmount();

    return 0;
}