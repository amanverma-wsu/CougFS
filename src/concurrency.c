#include <stdio.h>
#include <string.h>
#include "concurrency.h"

static pthread_rwlock_t fs_lock;
static pthread_rwlock_t inode_locks[MAX_INODES];
static int initialized = 0;

int concurrency_init(void)
{
    if (initialized)
        return 0;
    if (pthread_rwlock_init(&fs_lock, NULL) != 0) {
        perror("concurrency_init: fs_lock");
        return -1;
    }
    for (uint32_t i = 0; i < MAX_INODES; i++) {
        if (pthread_rwlock_init(&inode_locks[i], NULL) != 0) {
            perror("concurrency_init: inode_lock");
            return -1;
        }
    }
    initialized = 1;
    return 0;
}

void concurrency_destroy(void)
{
    if (!initialized)
        return;
    pthread_rwlock_destroy(&fs_lock);
    for (uint32_t i = 0; i < MAX_INODES; i++) {
        pthread_rwlock_destroy(&inode_locks[i]);
    }
    initialized = 0;
}

void fs_read_lock(void)   { pthread_rwlock_rdlock(&fs_lock); }
void fs_read_unlock(void) { pthread_rwlock_unlock(&fs_lock); }
void fs_write_lock(void)  { pthread_rwlock_wrlock(&fs_lock); }
void fs_write_unlock(void){ pthread_rwlock_unlock(&fs_lock); }

void inode_read_lock(uint32_t ino)
{
    if (ino < MAX_INODES) pthread_rwlock_rdlock(&inode_locks[ino]);
}

void inode_read_unlock(uint32_t ino)
{
    if (ino < MAX_INODES) pthread_rwlock_unlock(&inode_locks[ino]);
}

void inode_write_lock(uint32_t ino)
{
    if (ino < MAX_INODES) pthread_rwlock_wrlock(&inode_locks[ino]);
}

void inode_write_unlock(uint32_t ino)
{
    if (ino < MAX_INODES) pthread_rwlock_unlock(&inode_locks[ino]);
}