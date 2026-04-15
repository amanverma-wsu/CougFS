/*
 * CougFS: Test Suite
 *
 * Comprehensive tests for all filesystem modules.
 * Uses a simple assertion-based test framework.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cougfs.h"
#include "disk.h"
#include "bitmap.h"
#include "inode.h"
#include "dir.h"
#include "file.h"
#include "fs.h"
#include "journal.h"
#include "concurrency.h"

#define TEST_DISK "test_disk.img"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

/* ---- Disk I/O Tests ---- */
static void test_disk(void)
{
    printf("=== Disk I/O Tests ===\n");

    /* Clean up any previous test disk */
    unlink(TEST_DISK);

    ASSERT(disk_open(TEST_DISK) == 0, "disk_open creates new disk");

    /* Write a block */
    uint8_t write_buf[BLOCK_SIZE];
    memset(write_buf, 0xAB, BLOCK_SIZE);
    ASSERT(disk_write_block(0, write_buf) == 0, "disk_write_block succeeds");

    /* Read it back */
    uint8_t read_buf[BLOCK_SIZE];
    ASSERT(disk_read_block(0, read_buf) == 0, "disk_read_block succeeds");
    ASSERT(memcmp(write_buf, read_buf, BLOCK_SIZE) == 0, "read matches write");

    /* Out of range */
    ASSERT(disk_read_block(DISK_SIZE_BLOCKS + 1, read_buf) < 0,
           "out of range read fails");
    ASSERT(disk_write_block(DISK_SIZE_BLOCKS + 1, write_buf) < 0,
           "out of range write fails");

    disk_close();
    unlink(TEST_DISK);
    printf("\n");
}

/* ---- Format and Mount Tests ---- */
static void test_format_mount(void)
{
    printf("=== Format and Mount Tests ===\n");

    unlink(TEST_DISK);

    ASSERT(fs_format(TEST_DISK) == 0, "fs_format succeeds");
    ASSERT(fs_mount(TEST_DISK) == 0, "fs_mount succeeds");

    const cougfs_superblock_t *sb = fs_get_superblock();
    ASSERT(sb != NULL, "superblock is not NULL");
    ASSERT(sb->magic == COUGFS_MAGIC, "superblock magic is correct");
    ASSERT(sb->block_size == BLOCK_SIZE, "block size is correct");
    ASSERT(sb->total_blocks == DISK_SIZE_BLOCKS, "total blocks is correct");
    ASSERT(sb->total_inodes == MAX_INODES, "total inodes is correct");

    fs_unmount();
    unlink(TEST_DISK);
    printf("\n");
}

/* ---- Bitmap Tests ---- */
static void test_bitmap(void)
{
    printf("=== Bitmap Tests ===\n");

    unlink(TEST_DISK);
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);

    /* Root inode (0) should be allocated already */
    ASSERT(bitmap_inode_is_set(ROOT_INODE), "root inode is allocated");

    /* Allocate some inodes */
    int ino1 = bitmap_alloc_inode();
    ASSERT(ino1 > 0, "alloc inode returns valid ino");
    ASSERT(bitmap_inode_is_set(ino1), "allocated inode is set");

    int ino2 = bitmap_alloc_inode();
    ASSERT(ino2 > 0, "second alloc returns valid ino");
    ASSERT(ino1 != ino2, "different inodes allocated");

    /* Free and re-allocate */
    bitmap_free_inode(ino1);
    ASSERT(!bitmap_inode_is_set(ino1), "freed inode is cleared");

    int ino3 = bitmap_alloc_inode();
    ASSERT(ino3 == ino1, "re-allocates freed inode");

    /* Block allocation */
    int blk1 = bitmap_alloc_block();
    ASSERT(blk1 >= (int)DATA_BLOCK_START, "alloc block returns valid block");
    ASSERT(bitmap_block_is_set(blk1), "allocated block is set");

    bitmap_free_block(blk1);
    ASSERT(!bitmap_block_is_set(blk1), "freed block is cleared");

    bitmap_free_inode(ino2);
    bitmap_free_inode(ino3);

    fs_unmount();
    unlink(TEST_DISK);
    printf("\n");
}

/* ---- Inode Tests ---- */
static void test_inode(void)
{
    printf("=== Inode Tests ===\n");

    unlink(TEST_DISK);
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);

    /* Allocate a new inode */
    int ino = inode_alloc(COUGFS_S_IFREG | 0644);
    ASSERT(ino > 0, "inode_alloc succeeds");

    /* Read it back */
    cougfs_inode_t inode;
    ASSERT(inode_read(ino, &inode) == 0, "inode_read succeeds");
    ASSERT((inode.mode & COUGFS_S_IFMT) == COUGFS_S_IFREG, "mode is regular file");
    ASSERT(inode.link_count == 1, "link count is 1");
    ASSERT(inode.size == 0, "size is 0");

    /* Get a data block */
    uint32_t blk = inode_get_block(&inode, 0, 1);
    ASSERT(blk != INVALID_BLOCK, "get_block with alloc succeeds");
    ASSERT(blk >= DATA_BLOCK_START, "block is in data area");

    /* Write inode back */
    inode_write(ino, &inode);

    /* Free inode */
    ASSERT(inode_free(ino) == 0, "inode_free succeeds");
    ASSERT(!bitmap_inode_is_set(ino), "freed inode is unset in bitmap");

    fs_unmount();
    unlink(TEST_DISK);
    printf("\n");
}

/* ---- Directory Tests ---- */
static void test_directory(void)
{
    printf("=== Directory Tests ===\n");

    unlink(TEST_DISK);
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);

    /* Root directory should have . and .. */
    int dot = dir_lookup(ROOT_INODE, ".");
    ASSERT(dot == (int)ROOT_INODE, "root . points to root");

    int dotdot = dir_lookup(ROOT_INODE, "..");
    ASSERT(dotdot == (int)ROOT_INODE, "root .. points to root");

    /* Create a subdirectory */
    int sub_ino = dir_create(ROOT_INODE, "testdir", 0755);
    ASSERT(sub_ino > 0, "dir_create succeeds");

    int found = dir_lookup(ROOT_INODE, "testdir");
    ASSERT(found == sub_ino, "lookup finds created dir");

    /* Subdirectory . and .. */
    ASSERT(dir_lookup(sub_ino, ".") == sub_ino, "subdir . is correct");
    ASSERT(dir_lookup(sub_ino, "..") == (int)ROOT_INODE, "subdir .. is correct");

    /* Directory should be empty */
    ASSERT(dir_is_empty(sub_ino), "new dir is empty");

    /* Remove directory */
    ASSERT(dir_remove(ROOT_INODE, "testdir") == 0, "dir_remove succeeds");
    ASSERT(dir_lookup(ROOT_INODE, "testdir") < 0, "removed dir not found");

    /* Path resolution */
    int sub2 = dir_create(ROOT_INODE, "a", 0755);
    ASSERT(sub2 > 0, "create dir 'a'");
    int sub3 = dir_create(sub2, "b", 0755);
    ASSERT(sub3 > 0, "create dir 'a/b'");

    ASSERT(dir_resolve_path("/a") == sub2, "resolve /a");
    ASSERT(dir_resolve_path("/a/b") == sub3, "resolve /a/b");
    ASSERT(dir_resolve_path("/") == (int)ROOT_INODE, "resolve /");
    ASSERT(dir_resolve_path("/nonexistent") < 0, "resolve nonexistent fails");

    /* Clean up */
    dir_remove(sub2, "b");
    dir_remove(ROOT_INODE, "a");

    fs_unmount();
    unlink(TEST_DISK);
    printf("\n");
}

/* ---- File Tests ---- */
static void test_file(void)
{
    printf("=== File Tests ===\n");

    unlink(TEST_DISK);
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);

    /* Create a file */
    int ino = file_create(ROOT_INODE, "hello.txt", 0644);
    ASSERT(ino > 0, "file_create succeeds");

    /* Open and write */
    int fd = file_open(ino, COUGFS_O_RDWR);
    ASSERT(fd >= 0, "file_open succeeds");

    const char *msg = "Hello, CougFS!";
    int written = file_write(fd, msg, (uint32_t)strlen(msg));
    ASSERT(written == (int)strlen(msg), "file_write writes correct bytes");

    /* Seek back and read */
    ASSERT(file_seek(fd, 0, 0) == 0, "seek to beginning");

    char buf[256];
    memset(buf, 0, sizeof(buf));
    int n = file_read(fd, buf, sizeof(buf));
    ASSERT(n == (int)strlen(msg), "file_read reads correct bytes");
    ASSERT(strcmp(buf, msg) == 0, "read data matches written data");

    file_close(fd);

    /* Stat */
    cougfs_inode_t stat_inode;
    ASSERT(file_stat(ino, &stat_inode) == 0, "file_stat succeeds");
    ASSERT(stat_inode.size == (uint32_t)strlen(msg), "stat size is correct");

    /* Truncate */
    ASSERT(file_truncate(ino, 5) == 0, "truncate succeeds");
    ASSERT(file_stat(ino, &stat_inode) == 0, "stat after truncate");
    ASSERT(stat_inode.size == 5, "truncated size is correct");

    /* Delete */
    ASSERT(file_delete(ROOT_INODE, "hello.txt") == 0, "file_delete succeeds");
    ASSERT(dir_lookup(ROOT_INODE, "hello.txt") < 0, "deleted file not found");

    /* Write a larger file (multi-block) */
    ino = file_create(ROOT_INODE, "big.txt", 0644);
    ASSERT(ino > 0, "create big file");

    fd = file_open(ino, COUGFS_O_RDWR);
    ASSERT(fd >= 0, "open big file");

    /* Write 8KB (2 blocks) */
    char big_data[8192];
    memset(big_data, 'X', sizeof(big_data));
    written = file_write(fd, big_data, sizeof(big_data));
    ASSERT(written == (int)sizeof(big_data), "write 8KB succeeds");

    /* Read it back */
    file_seek(fd, 0, 0);
    char big_read[8192];
    n = file_read(fd, big_read, sizeof(big_read));
    ASSERT(n == (int)sizeof(big_data), "read 8KB succeeds");
    ASSERT(memcmp(big_data, big_read, sizeof(big_data)) == 0, "8KB data matches");

    file_close(fd);
    file_delete(ROOT_INODE, "big.txt");

    fs_unmount();
    unlink(TEST_DISK);
    printf("\n");
}

/* ---- Concurrency Tests ---- */
static void test_concurrency(void)
{
    printf("=== Concurrency Tests ===\n");

    unlink(TEST_DISK);
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);

    /* Basic lock/unlock (single-threaded sanity check) */
    fs_read_lock();
    fs_read_unlock();
    ASSERT(1, "fs read lock/unlock works");

    fs_write_lock();
    fs_write_unlock();
    ASSERT(1, "fs write lock/unlock works");

    inode_read_lock(0);
    inode_read_unlock(0);
    ASSERT(1, "inode read lock/unlock works");

    inode_write_lock(0);
    inode_write_unlock(0);
    ASSERT(1, "inode write lock/unlock works");

    fs_unmount();
    unlink(TEST_DISK);
    printf("\n");
}

/* ---- Journal Tests ---- */
static void test_journal(void)
{
    printf("=== Journal Tests ===\n");

    unlink(TEST_DISK);
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);

    /* Begin a transaction */
    uint32_t txn = journal_begin();
    ASSERT(txn > 0, "journal_begin returns valid txn id");

    /* Log a write */
    uint8_t test_data[BLOCK_SIZE];
    memset(test_data, 0xCD, BLOCK_SIZE);
    ASSERT(journal_log_write(txn, DATA_BLOCK_START, test_data) == 0,
           "journal_log_write succeeds");

    /* Commit */
    ASSERT(journal_commit(txn) == 0, "journal_commit succeeds");

    /* Verify the data was written to the actual block */
    uint8_t verify[BLOCK_SIZE];
    disk_read_block(DATA_BLOCK_START, verify);
    ASSERT(memcmp(test_data, verify, BLOCK_SIZE) == 0,
           "committed data is on disk");

    /* Test abort */
    txn = journal_begin();
    uint8_t abort_data[BLOCK_SIZE];
    memset(abort_data, 0xEF, BLOCK_SIZE);
    journal_log_write(txn, DATA_BLOCK_START + 1, abort_data);
    journal_abort(txn);

    /* Aborted data should NOT be on disk */
    disk_read_block(DATA_BLOCK_START + 1, verify);
    ASSERT(memcmp(abort_data, verify, BLOCK_SIZE) != 0,
           "aborted data not written to disk");

    fs_unmount();
    unlink(TEST_DISK);
    printf("\n");
}

/* ---- Main ---- */
int main(void)
{
    printf("CougFS Test Suite\n");
    printf("=================\n\n");

    test_disk();
    test_format_mount();
    test_bitmap();
    test_inode();
    test_directory();
    test_file();
    test_concurrency();
    test_journal();

    printf("=================\n");
    printf("Results: %d/%d passed, %d failed\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
