#include <stdio.h>
#include <string.h>
#include "bitmap.h"
#include "disk.h"

static uint8_t inode_bitmap[BLOCK_SIZE];
static uint8_t data_bitmap[BLOCK_SIZE];

static inline void bit_set(uint8_t *bitmap, uint32_t bit)
{
    bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bit_clear(uint8_t *bitmap, uint32_t bit)
{
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline int bit_test(const uint8_t *bitmap, uint32_t bit)
{
    return (bitmap[bit / 8] >> (bit % 8)) & 1;
}

int bitmap_init(void)
{
    if (disk_read_block(INODE_BITMAP_BLOCK, inode_bitmap) < 0)
        return -1;
    if (disk_read_block(DATA_BITMAP_BLOCK, data_bitmap) < 0)
        return -1;
    return 0;
}

int bitmap_sync(void)
{
    /* TODO: implement */
    return -1;
}

int bitmap_alloc_inode(void)
{
    for (uint32_t i = 0; i < MAX_INODES; i++) {
        if (!bit_test(inode_bitmap, i)) {
            bit_set(inode_bitmap, i);
            return (int)i;
        }
    }
    return -1;
}

void bitmap_free_inode(uint32_t ino)
{
    if (ino < MAX_INODES)
        bit_clear(inode_bitmap, ino);
}

int bitmap_inode_is_set(uint32_t ino)
{
    if (ino >= MAX_INODES)
        return 0;
    return bit_test(inode_bitmap, ino);
}

uint32_t bitmap_free_inode_count(void)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_INODES; i++) {
        if (!bit_test(inode_bitmap, i))
            count++;
    }
    return count;
}

/* Data block functions - TODO */
int bitmap_alloc_block(void) { return -1; }
void bitmap_free_block(uint32_t block_num) { (void)block_num; }
int bitmap_block_is_set(uint32_t block_num) { (void)block_num; return 0; }
uint32_t bitmap_free_block_count(void) { return 0; }