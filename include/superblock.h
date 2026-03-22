#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include "cougfs.h"

/* Đọc superblock từ block 0. Trả 0 nếu OK, -1 nếu lỗi/không hợp lệ. */
int superblock_load(cougfs_superblock_t *sb);

/* Format filesystem mới trên disk.img. Trả 0 nếu OK. */
int superblock_format(cougfs_superblock_t *sb);

/* Ghi superblock hiện tại xuống block 0. */
int superblock_sync(const cougfs_superblock_t *sb);

#endif
