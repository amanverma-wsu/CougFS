# CougFS: A User-Space Unix-Like File System

A simplified Unix-like file system implementation in C that runs entirely in user space using a virtual disk image.

## Features
- Superblock and inode table management
- File creation, reading, writing, and deletion
- Directory creation and navigation
- Persistent storage via virtual disk image (disk.img)
- Command-line interface

## Prerequisites

Before building CougFS, ensure you have the following installed:
- GCC compiler
- Make build system
- Git (for cloning the repository)

## Installation

### 1. Clone the Repository
```bash
git clone https://github.com/amanverma-wsu/CougFS.git
cd CougFS
```

### 2. Build the Project
```bash
make
```

This will:
- Compile all source files from the `src/` directory
- Create a `build/` directory
- Generate the executable `build/cougfs`

### 3. Run Tests
```bash
make test
```

### 4. Clean Build Artifacts
To remove all build artifacts and generated files:
```bash
make clean
```

This removes the `build/` directory, object files, and `disk.img`.

## Usage

Run the file system:
```bash
./build/cougfs
```

This will format and launch the CougFS command-line interface where you can interact with the virtual file system.

## Project Structure

```
CougFS/
├── src/          # Source files
├── include/      # Header files
├── tests/        # Test files
├── docs/         # Documentation
├── build/        # Build output (generated)
├── Makefile      # Build configuration
└── README.md     # This file
```

## Team
- **Aman Verma** (amanverma-wsu) - Lead
- **Tony Cao** (Tony2K3)
- **Srishanth Reddy Surakanti** (SURAKANTISRISHANTHREDDY)
- **Alan Qiu** (AlanRQiu)
