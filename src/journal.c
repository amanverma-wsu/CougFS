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
    if (!active_txn.active || active_txn.seq != txn_id)
        return -1;
    uint32_t journal_block = JOURNAL_START + 1;
    for (int i = 0; i < active_txn.count; i++) {
        if (journal_block >= JOURNAL_START + JOURNAL_BLOCKS) {
            fprintf(stderr, "journal_commit: journal full\n");
            journal_abort(txn_id);
            return -1;
        }
        uint8_t entry_block[BLOCK_SIZE];
        memset(entry_block, 0, BLOCK_SIZE);
        journal_entry_t *entry = (journal_entry_t *)entry_block;
        entry->seq = txn_id;
        entry->op = JOURNAL_OP_WRITE;
        entry->block_num = active_txn.block_nums[i];
        entry->data_len = BLOCK_SIZE - sizeof(journal_entry_t);
        memcpy(entry_block + sizeof(journal_entry_t),
               active_txn.data[i],
               BLOCK_SIZE - sizeof(journal_entry_t));
        if (disk_write_block(journal_block, entry_block) < 0)
            return -1;
        journal_block++;
    }
    if (journal_block < JOURNAL_START + JOURNAL_BLOCKS) {
        uint8_t commit_block[BLOCK_SIZE];
        memset(commit_block, 0, BLOCK_SIZE);
        journal_entry_t *commit = (journal_entry_t *)commit_block;
        commit->seq = txn_id;
        commit->op = JOURNAL_OP_COMMIT;
        commit->block_num = 0;
        commit->data_len = 0;
        disk_write_block(journal_block, commit_block);
    }
    disk_sync();
    for (int i = 0; i < active_txn.count; i++) {
        disk_write_block(active_txn.block_nums[i], active_txn.data[i]);
    }
    disk_sync();
    jheader.seq = current_seq;
    jheader.head = 0;
    jheader.tail = 0;
    disk_write_block(JOURNAL_START, &jheader);
    uint8_t zeros[BLOCK_SIZE];
    memset(zeros, 0, BLOCK_SIZE);
    for (uint32_t b = JOURNAL_START + 1; b < JOURNAL_START + JOURNAL_BLOCKS; b++) {
        disk_write_block(b, zeros);
    }
    disk_sync();
    active_txn.active = 0;
    return 0;
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
    printf("CougFS journal: scanning for uncommitted transactions...\n");
    int found_commit = 0;
    int entry_count = 0;
    uint32_t committed_seq = 0;
    for (uint32_t b = JOURNAL_START + 1; b < JOURNAL_START + JOURNAL_BLOCKS; b++) {
        uint8_t block[BLOCK_SIZE];
        if (disk_read_block(b, block) < 0)
            continue;
        journal_entry_t *entry = (journal_entry_t *)block;
        if (entry->op == JOURNAL_OP_COMMIT && entry->seq > 0) {
            found_commit = 1;
            committed_seq = entry->seq;
            break;
        }
        if (entry->op == JOURNAL_OP_WRITE && entry->seq > 0) {
            entry_count++;
        }
    }
    if (!found_commit) {
        printf("CougFS journal: no committed transactions to replay\n");
        uint8_t zeros[BLOCK_SIZE];
        memset(zeros, 0, BLOCK_SIZE);
        for (uint32_t b = JOURNAL_START + 1; b < JOURNAL_START + JOURNAL_BLOCKS; b++) {
            disk_write_block(b, zeros);
        }
        return 0;
    }
    printf("CougFS journal: replaying transaction %u (%d entries)\n",
           committed_seq, entry_count);
    for (uint32_t b = JOURNAL_START + 1; b < JOURNAL_START + JOURNAL_BLOCKS; b++) {
        uint8_t block[BLOCK_SIZE];
        if (disk_read_block(b, block) < 0)
            continue;
        journal_entry_t *entry = (journal_entry_t *)block;
        if (entry->op == JOURNAL_OP_WRITE && entry->seq == committed_seq) {
            uint8_t restored[BLOCK_SIZE];
            memset(restored, 0, BLOCK_SIZE);
            memcpy(restored, block + sizeof(journal_entry_t),
                   BLOCK_SIZE - sizeof(journal_entry_t));
            printf("  replaying block %u\n", entry->block_num);
            disk_write_block(entry->block_num, restored);
        }
        if (entry->op == JOURNAL_OP_COMMIT)
            break;
    }
    uint8_t zeros[BLOCK_SIZE];
    memset(zeros, 0, BLOCK_SIZE);
    for (uint32_t b = JOURNAL_START + 1; b < JOURNAL_START + JOURNAL_BLOCKS; b++) {
        disk_write_block(b, zeros);
    }
    jheader.seq = committed_seq + 1;
    jheader.head = 0;
    jheader.tail = 0;
    disk_write_block(JOURNAL_START, &jheader);
    disk_sync();
    printf("CougFS journal: recovery complete\n");
    return 0;
}

void journal_sync(void)
{
    disk_write_block(JOURNAL_START, &jheader);
    disk_sync();
}