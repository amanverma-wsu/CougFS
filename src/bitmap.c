#include <stdio.h>
#include <string.h>
#include "bitmap.h"
#include "disk.h"

static uint8_t inode_bitmap[BLOCK_SIZE];
static uint8_t data_bitmap[BLOCK_SIZE];
static uint32_t free_inode_count;
static uint32_t free_block_count;

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

/* Count free (unset) bits in a bitmap range */
static uint32_t bitmap_count_free(const uint8_t *bitmap, uint32_t max)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < max; i++) {
        if (!bit_test(bitmap, i))
            count++;
    }
    return count;
}

/* Find and allocate the first free bit in a bitmap.
 * Sets the bit and stores its index in *out.
 * Returns 0 on success, -1 if no free bit found. */
static int bitmap_find_free_bit(uint8_t *bitmap, uint32_t max, uint32_t *out)
{
    for (uint32_t i = 0; i < max; i++) {
        if (!bit_test(bitmap, i)) {
            bit_set(bitmap, i);
            *out = i;
            return 0;
        }
    }
    return -1;
}

int bitmap_init(void)
{
    if (disk_read_block(INODE_BITMAP_BLOCK, inode_bitmap) < 0)
        return -1;
    if (disk_read_block(DATA_BITMAP_BLOCK, data_bitmap) < 0)
        return -1;
    /* Initialize free counters by scanning bitmaps once */
    free_inode_count = bitmap_count_free(inode_bitmap, MAX_INODES);
    free_block_count = bitmap_count_free(data_bitmap, MAX_DATA_BLOCKS);
    return 0;
}

int bitmap_sync(void)
{
    if (disk_write_block(INODE_BITMAP_BLOCK, inode_bitmap) < 0)
        return -1;
    if (disk_write_block(DATA_BITMAP_BLOCK, data_bitmap) < 0)
        return -1;
    return 0;
}

int bitmap_alloc_inode(void)
{
    uint32_t ino;
    if (bitmap_find_free_bit(inode_bitmap, MAX_INODES, &ino) == 0) {
        if (free_inode_count > 0)
            free_inode_count--;
        return (int)ino;
    }
    return -1;
}

void bitmap_free_inode(uint32_t ino)
{
    if (ino < MAX_INODES) {
        bit_clear(inode_bitmap, ino);
        if (free_inode_count < MAX_INODES)
            free_inode_count++;
    }
}

int bitmap_inode_is_set(uint32_t ino)
{
    if (ino >= MAX_INODES)
        return 0;
    return bit_test(inode_bitmap, ino);
}

uint32_t bitmap_free_inode_count(void)
{
    return free_inode_count;
}

int bitmap_alloc_block(void)
{
    uint32_t idx;
    if (bitmap_find_free_bit(data_bitmap, MAX_DATA_BLOCKS, &idx) == 0) {
        if (free_block_count > 0)
            free_block_count--;
        return (int)(idx + DATA_BLOCK_START);
    }
    return -1;
}

void bitmap_free_block(uint32_t block_num)
{
    if (block_num >= DATA_BLOCK_START && block_num < DISK_SIZE_BLOCKS) {
        uint32_t idx = block_num - DATA_BLOCK_START;
        bit_clear(data_bitmap, idx);
        if (free_block_count < MAX_DATA_BLOCKS)
            free_block_count++;
    }
}

int bitmap_block_is_set(uint32_t block_num)
{
    if (block_num < DATA_BLOCK_START || block_num >= DISK_SIZE_BLOCKS)
        return 0;
    uint32_t idx = block_num - DATA_BLOCK_START;
    return bit_test(data_bitmap, idx);
}

uint32_t bitmap_free_block_count(void)
{
    return free_block_count;
}