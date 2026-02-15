# CougFS: A User-Space Unix-Like File System

A simplified Unix-like file system implementation in C that runs entirely in user space using a virtual disk image.

## Features
- Superblock and inode table management
- File creation, reading, writing, and deletion
- Directory creation and navigation
- Persistent storage via virtual disk image (disk.img)
- Command-line interface

## Quick Start
\`\`\`bash
# Clone the repository
git clone https://github.com/amanverma-wsu/CougFS.git
cd CougFS

# Build
make

# Run tests
make test

# Format and explore the file system
./build/cougfs
\`\`\`

## Team
- **Aman Verma** (amanverma-wsu) - Lead
- **Tony Cao** (Tony2K3)
- **Srishanth Reddy Surakanti** (SURAKANTISRISHANTHREDDY)
- **Alan Qiu** (AlanRQiu)

## Documentation
- [Design & Architecture](docs/DESIGN.md)
- [API Reference](docs/API.md)
- [Development Guide](docs/DEVELOPMENT.md)

## License
MIT License - See LICENSE file
