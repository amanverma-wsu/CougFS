#include <stdio.h>
#include <string.h>
#include "journal.h"
#include "disk.h"

static journal_header_t jheader;
static uint32_t current_seq;

#define MAX_TXN_ENTRIES 16

typedef struct {
    int      active;
    uint32_t seq;
    int      count;
    uint32_t block_nums[MAX_TXN_ENTRIES];
    uint8_t  data[MAX_TXN_ENTRIES][BLOCK_SIZE];
} txn_buffer_t;

static txn_buffer_t active_txn;

int journal_init(void)
{
    if (disk_read_block(JOURNAL_START, &jheader) < 0)
        return -1;
    if (jheader.magic != COUGFS_MAGIC) {
        memset(&jheader, 0, sizeof(jheader));
        jheader.magic = COUGFS_MAGIC;
        jheader.head = 0;
        jheader.tail = 0;
        jheader.seq = 1;
        disk_write_block(JOURNAL_START, &jheader);
    }
    current_seq = jheader.seq;
    memset(&active_txn, 0, sizeof(active_txn));
    return 0;
}

uint32_t journal_begin(void)
{
    active_txn.active = 1;
    active_txn.seq = current_seq++;
    active_txn.count = 0;
    return active_txn.seq;
}

int journal_log_write(uint32_t txn_id, uint32_t block_num, const void *data)
{
    if (!active_txn.active || active_txn.seq != txn_id)
        return -1;
    if (active_txn.count >= MAX_TXN_ENTRIES)
        return -1;
    int idx = active_txn.count++;
    active_txn.block_nums[idx] = block_num;
    memcpy(active_txn.data[idx], data, BLOCK_SIZE);
    return 0;
}

int journal_commit(uint32_t txn_id)
{
    /* TODO: implement */
    (void)txn_id;
    return -1;
}

void journal_abort(uint32_t txn_id)
{
    if (active_txn.active && active_txn.seq == txn_id) {
        active_txn.active = 0;
        active_txn.count = 0;
    }
}

int journal_recover(void)
{
    /* TODO: implement */
    return -1;
}

void journal_sync(void)
{
    disk_write_block(JOURNAL_START, &jheader);
    disk_sync();
}