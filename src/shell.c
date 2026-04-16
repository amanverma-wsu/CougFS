/*
 * CougFS: Interactive Shell
 *
 * Provides a command-line interface for interacting with the filesystem.
 * Supports commands: ls, cd, mkdir, rmdir, touch, cat, write, rm, stat,
 * truncate, cp, pwd, help, quit.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cougfs.h"
#include "fs.h"
#include "dir.h"
#include "file.h"
#include "inode.h"
#include "bitmap.h"
#include "concurrency.h"

static uint32_t cwd_ino = ROOT_INODE;
static char cwd_path[1024] = "/";

/* Callback for ls command */
static void ls_callback(const char *name, uint32_t ino, uint8_t file_type, void *ctx)
{
    (void)ctx;
    cougfs_inode_t inode;
    if (inode_read(ino, &inode) < 0) {
        printf("  %-20s  [error reading inode %u]\n", name, ino);
        return;
    }

    char type = '-';
    if (file_type == DT_DIR || (inode.mode & COUGFS_S_IFMT) == COUGFS_S_IFDIR)
        type = 'd';

    char perms[10];
    perms[0] = (inode.mode & COUGFS_S_IRUSR) ? 'r' : '-';
    perms[1] = (inode.mode & COUGFS_S_IWUSR) ? 'w' : '-';
    perms[2] = (inode.mode & COUGFS_S_IXUSR) ? 'x' : '-';
    perms[3] = (inode.mode & 0040) ? 'r' : '-';
    perms[4] = (inode.mode & 0020) ? 'w' : '-';
    perms[5] = (inode.mode & 0010) ? 'x' : '-';
    perms[6] = (inode.mode & 0004) ? 'r' : '-';
    perms[7] = (inode.mode & 0002) ? 'w' : '-';
    perms[8] = (inode.mode & 0001) ? 'x' : '-';
    perms[9] = '\0';

    time_t mtime = (time_t)inode.mtime;
    struct tm *tm = localtime(&mtime);
    char timestr[32];
    strftime(timestr, sizeof(timestr), "%b %d %H:%M", tm);

    printf("  %c%s  %2u  %8u  %s  %s\n",
           type, perms, inode.link_count, inode.size, timestr, name);
}

static void cmd_ls(const char *args)
{
    uint32_t target = cwd_ino;
    if (args && args[0]) {
        int ino = dir_resolve_path_from(cwd_ino, args);
        if (ino < 0) {
            printf("ls: cannot access '%s': No such file or directory\n", args);
            return;
        }
        target = (uint32_t)ino;
    }

    fs_read_lock();
    dir_list(target, ls_callback, NULL);
    fs_read_unlock();
}

static void cmd_cd(const char *args)
{
    if (!args || !args[0]) {
        cwd_ino = ROOT_INODE;
        strcpy(cwd_path, "/");
        return;
    }

    int ino = dir_resolve_path_from(cwd_ino, args);
    if (ino < 0) {
        printf("cd: no such directory: %s\n", args);
        return;
    }

    cougfs_inode_t inode;
    if (inode_read(ino, &inode) < 0) {
        printf("cd: error reading inode\n");
        return;
    }

    if (!(inode.mode & COUGFS_S_IFDIR)) {
        printf("cd: not a directory: %s\n", args);
        return;
    }

    cwd_ino = (uint32_t)ino;

    /* Update cwd_path */
    if (args[0] == '/') {
        strncpy(cwd_path, args, sizeof(cwd_path) - 1);
    } else if (strcmp(args, "..") == 0) {
        char *last_slash = strrchr(cwd_path, '/');
        if (last_slash && last_slash != cwd_path)
            *last_slash = '\0';
        else
            strcpy(cwd_path, "/");
    } else if (strcmp(args, ".") != 0) {
        if (strcmp(cwd_path, "/") != 0)
            strncat(cwd_path, "/", sizeof(cwd_path) - strlen(cwd_path) - 1);
        strncat(cwd_path, args, sizeof(cwd_path) - strlen(cwd_path) - 1);
    }
}

static void cmd_mkdir(const char *args)
{
    if (!args || !args[0]) {
        printf("Usage: mkdir <dirname>\n");
        return;
    }

    fs_write_lock();
    int result = dir_create(cwd_ino, args, 0755);
    fs_write_unlock();

    if (result < 0)
        printf("mkdir: cannot create directory '%s'\n", args);
    else
        printf("Created directory '%s'\n", args);
}

static void cmd_rmdir(const char *args)
{
    if (!args || !args[0]) {
        printf("Usage: rmdir <dirname>\n");
        return;
    }

    fs_write_lock();
    int result = dir_remove(cwd_ino, args);
    fs_write_unlock();

    if (result < 0)
        printf("rmdir: cannot remove '%s': Directory not empty or not found\n", args);
    else
        printf("Removed directory '%s'\n", args);
}

static void cmd_touch(const char *args)
{
    if (!args || !args[0]) {
        printf("Usage: touch <filename>\n");
        return;
    }

    /* Check if file already exists */
    int existing = dir_lookup(cwd_ino, args);
    if (existing >= 0) {
        /* Just update mtime */
        cougfs_inode_t inode;
        if (inode_read(existing, &inode) == 0) {
            inode.mtime = (uint32_t)time(NULL);
            inode_write(existing, &inode);
        }
        return;
    }

    fs_write_lock();
    int ino = file_create(cwd_ino, args, 0644);
    fs_write_unlock();

    if (ino < 0)
        printf("touch: cannot create '%s'\n", args);
}

static void cmd_cat(const char *args)
{
    if (!args || !args[0]) {
        printf("Usage: cat <filename>\n");
        return;
    }

    int ino = dir_resolve_path_from(cwd_ino, args);
    if (ino < 0) {
        printf("cat: %s: No such file\n", args);
        return;
    }

    fs_read_lock();
    int fd = file_open(ino, COUGFS_O_RDONLY);
    if (fd < 0) {
        printf("cat: cannot open '%s'\n", args);
        fs_read_unlock();
        return;
    }

    char buf[BLOCK_SIZE];
    int n;
    while ((n = file_read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }
    printf("\n");

    file_close(fd);
    fs_read_unlock();
}

static void cmd_write(const char *args)
{
    if (!args || !args[0]) {
        printf("Usage: write <filename> <text>\n");
        return;
    }

    /* Split args into filename and text */
    char filename[MAX_NAME_LEN + 1];
    const char *text = NULL;

    const char *space = strchr(args, ' ');
    if (!space) {
        printf("Usage: write <filename> <text>\n");
        return;
    }

    size_t name_len = (size_t)(space - args);
    if (name_len > MAX_NAME_LEN) name_len = MAX_NAME_LEN;
    strncpy(filename, args, name_len);
    filename[name_len] = '\0';
    text = space + 1;

    /* Open or create file */
    int ino = dir_lookup(cwd_ino, filename);
    if (ino < 0) {
        fs_write_lock();
        ino = file_create(cwd_ino, filename, 0644);
        fs_write_unlock();
        if (ino < 0) {
            printf("write: cannot create '%s'\n", filename);
            return;
        }
    }

    fs_write_lock();
    int fd = file_open(ino, COUGFS_O_WRONLY | COUGFS_O_TRUNC);
    if (fd < 0) {
        printf("write: cannot open '%s'\n", filename);
        fs_write_unlock();
        return;
    }

    int written = file_write(fd, text, (uint32_t)strlen(text));
    file_close(fd);
    fs_write_unlock();

    if (written < 0)
        printf("write: error writing to '%s'\n", filename);
    else
        printf("Wrote %d bytes to '%s'\n", written, filename);
}

static void cmd_rm(const char *args)
{
    if (!args || !args[0]) {
        printf("Usage: rm <filename>\n");
        return;
    }

    fs_write_lock();
    int result = file_delete(cwd_ino, args);
    fs_write_unlock();

    if (result < 0)
        printf("rm: cannot remove '%s'\n", args);
    else
        printf("Removed '%s'\n", args);
}

static void cmd_stat(const char *args)
{
    if (!args || !args[0]) {
        printf("Usage: stat <name>\n");
        return;
    }

    int ino = dir_resolve_path_from(cwd_ino, args);
    if (ino < 0) {
        printf("stat: cannot stat '%s': No such file or directory\n", args);
        return;
    }

    cougfs_inode_t inode;
    if (inode_read(ino, &inode) < 0) {
        printf("stat: error reading inode\n");
        return;
    }

    const char *type = "unknown";
    if ((inode.mode & COUGFS_S_IFMT) == COUGFS_S_IFREG)
        type = "regular file";
    else if ((inode.mode & COUGFS_S_IFMT) == COUGFS_S_IFDIR)
        type = "directory";

    printf("  File: %s\n", args);
    printf("  Type: %s\n", type);
    printf("  Inode: %d\n", ino);
    printf("  Mode: 0%o\n", inode.mode & 0777);
    printf("  Links: %u\n", inode.link_count);
    printf("  Size: %u bytes\n", inode.size);
    printf("  Blocks: %u\n", inode.blocks);

    time_t atime = (time_t)inode.atime;
    time_t mtime = (time_t)inode.mtime;
    time_t ctime = (time_t)inode.ctime;
    printf("  Access: %s", ctime_r(&atime, (char[26]){}));
    printf("  Modify: %s", ctime_r(&mtime, (char[26]){}));
    printf("  Create: %s", ctime_r(&ctime, (char[26]){}));
}

static void cmd_truncate(const char *args)
{
    if (!args || !args[0]) {
        printf("Usage: truncate <filename> <size>\n");
        return;
    }

    char filename[MAX_NAME_LEN + 1];
    uint32_t new_size = 0;

    if (sscanf(args, "%251s %u", filename, &new_size) < 2) {
        printf("Usage: truncate <filename> <size>\n");
        return;
    }

    int ino = dir_resolve_path_from(cwd_ino, filename);
    if (ino < 0) {
        printf("truncate: '%s': No such file\n", filename);
        return;
    }

    fs_write_lock();
    int result = file_truncate(ino, new_size);
    fs_write_unlock();

    if (result < 0)
        printf("truncate: error truncating '%s'\n", filename);
    else
        printf("Truncated '%s' to %u bytes\n", filename, new_size);
}

static void cmd_info(void)
{
    const cougfs_superblock_t *sb = fs_get_superblock();
    printf("CougFS Filesystem Info:\n");
    printf("  Magic:        0x%08X\n", sb->magic);
    printf("  Version:      %u\n", sb->version);
    printf("  Block size:   %u bytes\n", sb->block_size);
    printf("  Total blocks: %u\n", sb->total_blocks);
    printf("  Total inodes: %u\n", sb->total_inodes);
    printf("  Free blocks:  %u\n", bitmap_free_block_count());
    printf("  Free inodes:  %u\n", bitmap_free_inode_count());
    printf("  Mount count:  %u\n", sb->mount_count);
    printf("  Data start:   block %u\n", sb->data_block_start);
}

static void cmd_help(void)
{
    printf("CougFS Shell Commands:\n");
    printf("  ls [path]              - List directory contents\n");
    printf("  cd [path]              - Change directory\n");
    printf("  pwd                    - Print working directory\n");
    printf("  mkdir <name>           - Create directory\n");
    printf("  rmdir <name>           - Remove empty directory\n");
    printf("  touch <name>           - Create empty file\n");
    printf("  cat <name>             - Display file contents\n");
    printf("  write <name> <text>    - Write text to file\n");
    printf("  rm <name>              - Delete file\n");
    printf("  stat <name>            - Show file/dir info\n");
    printf("  truncate <name> <size> - Truncate file\n");
    printf("  info                   - Show filesystem info\n");
    printf("  help                   - Show this help\n");
    printf("  quit                   - Exit shell\n");
}

void shell_run(void)
{
    char line[1024];
    char cmd[64];
    char *args;

    printf("CougFS Shell - Type 'help' for commands\n\n");

    while (1) {
        printf("cougfs:%s$ ", cwd_path);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            break;

        /* Strip newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

        /* Skip empty lines */
        if (line[0] == '\0')
            continue;

        /* Parse command and arguments */
        args = strchr(line, ' ');
        if (args) {
            size_t cmd_len = (size_t)(args - line);
            if (cmd_len >= sizeof(cmd)) cmd_len = sizeof(cmd) - 1;
            strncpy(cmd, line, cmd_len);
            cmd[cmd_len] = '\0';
            args++;
            /* Skip leading spaces in args */
            while (*args == ' ') args++;
        } else {
            strncpy(cmd, line, sizeof(cmd) - 1);
            cmd[sizeof(cmd) - 1] = '\0';
            args = NULL;
        }

        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0)
            break;
        else if (strcmp(cmd, "help") == 0)
            cmd_help();
        else if (strcmp(cmd, "ls") == 0)
            cmd_ls(args);
        else if (strcmp(cmd, "cd") == 0)
            cmd_cd(args);
        else if (strcmp(cmd, "pwd") == 0)
            printf("%s\n", cwd_path);
        else if (strcmp(cmd, "mkdir") == 0)
            cmd_mkdir(args);
        else if (strcmp(cmd, "rmdir") == 0)
            cmd_rmdir(args);
        else if (strcmp(cmd, "touch") == 0)
            cmd_touch(args);
        else if (strcmp(cmd, "cat") == 0)
            cmd_cat(args);
        else if (strcmp(cmd, "write") == 0)
            cmd_write(args);
        else if (strcmp(cmd, "rm") == 0)
            cmd_rm(args);
        else if (strcmp(cmd, "stat") == 0)
            cmd_stat(args);
        else if (strcmp(cmd, "truncate") == 0)
            cmd_truncate(args);
        else if (strcmp(cmd, "info") == 0)
            cmd_info();
        else
            printf("Unknown command: %s (type 'help' for available commands)\n", cmd);
    }
}
