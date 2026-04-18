# CougFS Design Document

## Overview

CougFS is a Unix-like filesystem implemented in user space on top of a fixed-size
virtual disk image. The codebase is organized around block I/O, metadata
management, directory and file operations, journaling, and a shell/FUSE frontend.

## Disk Layout

- Block 0: superblock
- Block 1: inode bitmap
- Block 2: data block bitmap
- Blocks 3-10: inode table
- Blocks 11-18: journal area
- Blocks 19-4095: data blocks

## Core Components

- `src/disk.c`: block-aligned reads, writes, sync, and disk image lifecycle
- `src/superblock.c` and `src/fs.c`: filesystem metadata initialization, mount,
  and unmount flows
- `src/bitmap.c`: inode and data block allocation tracking
- `src/inode.c`: inode allocation, persistence, and block mapping
- `src/dir.c`: directory entries, path resolution, and directory mutation
- `src/file.c`: file creation, open/read/write/truncate/remove operations
- `src/journal.c`: write-ahead journal and recovery path
- `src/concurrency.c`: reader/writer locking for thread-safe access
- `src/shell.c` and `src/main.c`: CLI entry points
- `src/fuse_ops.c`: optional FUSE integration

## Format and Mount Flow

`fs_format()` initializes the superblock, clears allocation state, zeroes the
journal, creates the root inode, and adds `.` and `..` entries. `fs_mount()`
validates the superblock, loads allocation metadata, initializes synchronization
primitives, and replays the journal if the filesystem was not cleanly unmounted.

## Testing

The main regression coverage lives in `tests/test_main.c`, which exercises disk
I/O, formatting/mounting, allocation, inode handling, directories, and file
operations through a single test binary built as `build/test_cougfs`.
