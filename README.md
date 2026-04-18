# CougFS: A User-Space Unix-Like File System

**WSU CPTS 360 - Systems Programming**

A complete, production-oriented Unix-like file system implemented in C that runs entirely in user space using a virtual disk image file (`disk.img`).

## Features

- **Superblock management** and filesystem initialization
- **Inode table** implementation with file metadata tracking
- **Block allocation/deallocation** using bitmaps
- **File operations**: create, read, write, delete, truncate, seek
- **Directory operations**: create, list, navigate, remove
- **Thread-safe concurrent access** using POSIX reader/writer locks
- **Crash recovery** using write-ahead journaling
- **FUSE integration** for mounting with standard Linux tools
- **Interactive shell** with Unix-like commands

## Prerequisites

- GCC compiler (C11 support)
- Make build system
- Git
- Linux/macOS environment
- libfuse-dev (optional, for FUSE support)

## Building

```bash
make            # Build the main binary
make test       # Build and run tests
make clean      # Remove build artifacts
```

### With FUSE support (optional)

```bash
sudo apt-get install libfuse-dev
make ENABLE_FUSE=1
```

## Usage

### Format a new filesystem
```bash
./build/cougfs format disk.img
```

### Mount and use interactive shell
```bash
./build/cougfs mount disk.img
```

### Mount via FUSE (if compiled with FUSE support)
```bash
mkdir /tmp/cougfs
./build/cougfs fuse disk.img /tmp/cougfs
# Use standard tools: ls, cp, cat, etc.
fusermount -u /tmp/cougfs
```

## Shell Commands

| Command | Description |
|---------|-------------|
| `ls [path]` | List directory contents |
| `cd [path]` | Change directory |
| `pwd` | Print working directory |
| `mkdir <name>` | Create directory |
| `rmdir <name>` | Remove empty directory |
| `touch <name>` | Create empty file |
| `cat <name>` | Display file contents |
| `write <name> <text>` | Write text to file |
| `rm <name>` | Delete file |
| `stat <name>` | Show file/directory info |
| `truncate <name> <size>` | Truncate file to size |
| `info` | Show filesystem info |
| `help` | Show available commands |
| `quit` | Exit shell |

## Disk Layout

| Blocks | Purpose |
|--------|---------|
| 0 | Superblock |
| 1 | Inode bitmap |
| 2 | Data block bitmap |
| 3-10 | Inode table (256 inodes) |
| 11-18 | Journal area |
| 19-4095 | Data blocks |

## Architecture

```
include/            Header files
  cougfs.h          Core data structures (superblock, inode, dir entry)
  disk.h            Disk I/O layer
  bitmap.h          Block/inode bitmap operations
  inode.h           Inode operations
  dir.h             Directory operations
  file.h            File operations
  fs.h              Filesystem init/mount
  journal.h         Journaling/crash recovery
  concurrency.h     Thread-safe locking
  fuse_ops.h        FUSE integration
  superblock.h      Superblock management

src/                Implementation files
  disk.c            Block-level read/write using POSIX syscalls
  bitmap.c          Inode and data block bitmap management
  inode.c           Inode read/write, alloc/free, block pointers
  dir.c             Directory create, lookup, list, remove, path resolution
  file.c            File create, open, read, write, seek, close, delete
  fs.c              Filesystem format, mount, unmount
  journal.c         Write-ahead journaling with crash recovery
  concurrency.c     POSIX rwlocks for thread safety
  superblock.c      Superblock load, format, sync
  shell.c           Interactive command-line interface
  main.c            Entry point with format/mount/fuse subcommands
  fuse_ops.c        FUSE callback implementation

tests/              Test suite
  test_main.c       Comprehensive tests for all modules
```

## Technical Stack

- **Language**: C (C11)
- **OS**: Linux
- **Compiler**: GCC with `-Wall -Wextra -Werror`
- **Libraries**: POSIX system calls, pthread, FUSE (optional)
- **Tools**: Make, Valgrind, GDB, GitHub Actions CI
- **Testing**: Custom assertion framework, 74 test cases, zero memory leaks

## Team

- **Aman Verma** (amanverma-wsu) - Lead
- **Tony Cao** (Tony2K3)
- **Srishanth Reddy Surakanti** (SURAKANTISRISHANTHREDDY)
- **Alan Qiu** (AlanRQiu)