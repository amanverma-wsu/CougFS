/*
 * CougFS: Journaling / Crash Recovery
 *
 * Write-ahead log for metadata operations. Each transaction records
 * block-level changes before they are committed to the main filesystem.
 */

#ifndef JOURNAL_H
#define JOURNAL_H

#include "cougfs.h"

/* Journal entry types */
#define JOURNAL_OP_NONE       0
#define JOURNAL_OP_WRITE      1   /* a block write */
#define JOURNAL_OP_COMMIT     2   /* transaction commit marker */

/* Journal header: stored at the start of the journal area */
typedef struct {
    uint32_t magic;               /* COUGFS_MAGIC */
    uint32_t head;                /* next write position (byte offset in journal) */
    uint32_t tail;                /* oldest uncommitted entry */
    uint32_t seq;                 /* sequence number */
    uint8_t  padding[BLOCK_SIZE - 16];
} journal_header_t;

/* Journal entry: describes one logged block write */
typedef struct {
    uint32_t seq;                 /* transaction sequence number */
    uint32_t op;                  /* operation type */
    uint32_t block_num;           /* target block on disk */
    uint32_t data_len;            /* bytes of data (up to BLOCK_SIZE) */
    /* Followed by data_len bytes of block data in the journal */
} journal_entry_t;

/* Initialize the journal. Returns 0 on success. */
int journal_init(void);

/* Begin a new transaction. Returns transaction ID. */
uint32_t journal_begin(void);

/* Log a block write within a transaction. Returns 0 on success. */
int journal_log_write(uint32_t txn_id, uint32_t block_num, const void *data);

/* Commit a transaction. Returns 0 on success. */
int journal_commit(uint32_t txn_id);

/* Abort a transaction (discard logged entries). */
void journal_abort(uint32_t txn_id);

/* Replay the journal to recover from a crash. Returns 0 on success. */
int journal_recover(void);

/* Sync journal to disk. */
void journal_sync(void);

#endif /* JOURNAL_H */
