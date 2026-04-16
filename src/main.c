#include <stdio.h>
#include <string.h>
#include "fs.h"
#include "fuse_ops.h"

extern void shell_run(void);

static void usage(const char *prog)
{
    printf("CougFS: A User-Space Unix-Like File System\n\n");
    printf("Usage:\n");
    printf("  %s format <disk.img>                    Format a new filesystem\n", prog);
    printf("  %s mount  <disk.img>                    Mount and open shell\n", prog);
    printf("  %s fuse   <disk.img> <mountpoint> ...   Mount via FUSE\n", prog);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }
    const char *command = argv[1];
    const char *disk_path = argv[2];
    if (strcmp(command, "format") == 0) {
        return fs_format(disk_path);
    } else if (strcmp(command, "mount") == 0) {
        if (fs_mount(disk_path) < 0)
            return 1;
        shell_run();
        fs_unmount();
        return 0;
    } else if (strcmp(command, "fuse") == 0) {
        if (argc < 4) {
            printf("Usage: %s fuse <disk.img> <mountpoint> [fuse-options]\n", argv[0]);
            return 1;
        }
        int fuse_argc = argc - 2;
        char **fuse_argv = argv + 2;
        fuse_argv[0] = argv[0];
        return cougfs_fuse_main(fuse_argc, fuse_argv, disk_path);
    } else {
        fprintf(stderr, "Unknown command: %s\n", command);
        usage(argv[0]);
        return 1;
    }
}
