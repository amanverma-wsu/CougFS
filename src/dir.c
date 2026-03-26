#include <stdio.h>
#include <string.h>
#include <time.h>
#include "dir.h"
#include "inode.h"
#include "disk.h"
#include "bitmap.h"

int dir_lookup(uint32_t dir_ino, const char *name)
{
    cougfs_inode_t dir_inode;
    if (inode_read(dir_ino, &dir_inode) < 0)
        return -1;
    if (!(dir_inode.mode & COUGFS_S_IFDIR))
        return -1;
    uint32_t num_blocks = (dir_inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    cougfs_dir_entry_t entries[DIR_ENTRIES_PER_BLOCK];
    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t blk = inode_get_block(&dir_inode, b, 0);
        if (blk == INVALID_BLOCK)
            continue;
        if (disk_read_block(blk, entries) < 0)
            continue;
        for (int i = 0; i < DIR_ENTRIES_PER_BLOCK; i++) {
            if (entries[i].inode != INVALID_INODE &&
                strcmp(entries[i].name, name) == 0) {
                return (int)entries[i].inode;
            }
        }
    }
    return -1;
}

int dir_add_entry(uint32_t dir_ino, const char *name, uint32_t child_ino, uint8_t file_type)
{
    if (strlen(name) > MAX_NAME_LEN)
        return -1;
    cougfs_inode_t dir_inode;
    if (inode_read(dir_ino, &dir_inode) < 0)
        return -1;
    uint32_t num_blocks = (dir_inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (num_blocks == 0)
        num_blocks = 0;
    cougfs_dir_entry_t entries[DIR_ENTRIES_PER_BLOCK];
    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t blk = inode_get_block(&dir_inode, b, 0);
        if (blk == INVALID_BLOCK)
            continue;
        if (disk_read_block(blk, entries) < 0)
            continue;
        for (int i = 0; i < DIR_ENTRIES_PER_BLOCK; i++) {
            if (entries[i].inode == INVALID_INODE) {
                entries[i].inode = child_ino;
                entries[i].file_type = file_type;
                entries[i].name_len = (uint8_t)strlen(name);
                strncpy(entries[i].name, name, MAX_NAME_LEN);
                entries[i].name[MAX_NAME_LEN] = '\0';
                disk_write_block(blk, entries);
                return 0;
            }
        }
    }
    uint32_t new_blk = inode_get_block(&dir_inode, num_blocks, 1);
    if (new_blk == INVALID_BLOCK)
        return -1;
    memset(entries, 0, sizeof(entries));
    for (int i = 0; i < DIR_ENTRIES_PER_BLOCK; i++)
        entries[i].inode = INVALID_INODE;
    entries[0].inode = child_ino;
    entries[0].file_type = file_type;
    entries[0].name_len = (uint8_t)strlen(name);
    strncpy(entries[0].name, name, MAX_NAME_LEN);
    entries[0].name[MAX_NAME_LEN] = '\0';
    disk_write_block(new_blk, entries);
    dir_inode.size = (num_blocks + 1) * BLOCK_SIZE;
    dir_inode.mtime = (uint32_t)time(NULL);
    inode_write(dir_ino, &dir_inode);
    bitmap_sync();
    return 0;
}

int dir_remove_entry(uint32_t dir_ino, const char *name)
{
    cougfs_inode_t dir_inode;
    if (inode_read(dir_ino, &dir_inode) < 0)
        return -1;
    uint32_t num_blocks = (dir_inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    cougfs_dir_entry_t entries[DIR_ENTRIES_PER_BLOCK];
    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t blk = inode_get_block(&dir_inode, b, 0);
        if (blk == INVALID_BLOCK)
            continue;
        if (disk_read_block(blk, entries) < 0)
            continue;
        for (int i = 0; i < DIR_ENTRIES_PER_BLOCK; i++) {
            if (entries[i].inode != INVALID_INODE &&
                strcmp(entries[i].name, name) == 0) {
                entries[i].inode = INVALID_INODE;
                entries[i].name[0] = '\0';
                entries[i].name_len = 0;
                disk_write_block(blk, entries);
                return 0;
            }
        }
    }
    return -1;
}

int dir_list(uint32_t dir_ino,
             void (*callback)(const char *name, uint32_t ino, uint8_t file_type, void *ctx),
             void *ctx)
{
    cougfs_inode_t dir_inode;
    if (inode_read(dir_ino, &dir_inode) < 0)
        return -1;
    uint32_t num_blocks = (dir_inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    cougfs_dir_entry_t entries[DIR_ENTRIES_PER_BLOCK];
    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t blk = inode_get_block(&dir_inode, b, 0);
        if (blk == INVALID_BLOCK)
            continue;
        if (disk_read_block(blk, entries) < 0)
            continue;
        for (int i = 0; i < DIR_ENTRIES_PER_BLOCK; i++) {
            if (entries[i].inode != INVALID_INODE) {
                callback(entries[i].name, entries[i].inode,
                         entries[i].file_type, ctx);
            }
        }
    }
    return 0;
}

int dir_is_empty(uint32_t dir_ino)
{
    cougfs_inode_t dir_inode;
    if (inode_read(dir_ino, &dir_inode) < 0)
        return 0;
    uint32_t num_blocks = (dir_inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    cougfs_dir_entry_t entries[DIR_ENTRIES_PER_BLOCK];
    int count = 0;
    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t blk = inode_get_block(&dir_inode, b, 0);
        if (blk == INVALID_BLOCK)
            continue;
        if (disk_read_block(blk, entries) < 0)
            continue;
        for (int i = 0; i < DIR_ENTRIES_PER_BLOCK; i++) {
            if (entries[i].inode != INVALID_INODE) {
                count++;
                if (count > 2)
                    return 0;
            }
        }
    }
    return 1;
}

/* TODO: implement */
int dir_create(uint32_t parent_ino, const char *name, uint16_t mode)
{
    (void)parent_ino; (void)name; (void)mode;
    return -1;
}

int dir_remove(uint32_t parent_ino, const char *name)
{
    (void)parent_ino; (void)name;
    return -1;
}

int dir_resolve_path(const char *path)
{
    (void)path;
    return -1;
}

int dir_resolve_path_from(uint32_t cwd_ino, const char *path)
{
    (void)cwd_ino; (void)path;
    return -1;
}