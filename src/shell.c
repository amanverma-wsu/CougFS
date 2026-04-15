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
    if (!args || !args[0]) { printf("Usage: mkdir <dirname>\n"); return; }
    fs_write_lock();
    int result = dir_create(cwd_ino, args, 0755);
    fs_write_unlock();
    if (result < 0) printf("mkdir: cannot create directory '%s'\n", args);
    else printf("Created directory '%s'\n", args);
}

static void cmd_rmdir(const char *args)
{
    if (!args || !args[0]) { printf("Usage: rmdir <dirname>\n"); return; }
    fs_write_lock();
    int result = dir_remove(cwd_ino, args);
    fs_write_unlock();
    if (result < 0) printf("rmdir: cannot remove '%s': Directory not empty or not found\n", args);
    else printf("Removed directory '%s'\n", args);
}

/* TODO: touch, cat, write, rm, stat, truncate, info, help */

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
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';
        if (line[0] == '\0')
            continue;
        args = strchr(line, ' ');
        if (args) {
            size_t cmd_len = (size_t)(args - line);
            if (cmd_len >= sizeof(cmd)) cmd_len = sizeof(cmd) - 1;
            strncpy(cmd, line, cmd_len);
            cmd[cmd_len] = '\0';
            args++;
            while (*args == ' ') args++;
        } else {
            strncpy(cmd, line, sizeof(cmd) - 1);
            cmd[sizeof(cmd) - 1] = '\0';
            args = NULL;
        }
        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) break;
        else if (strcmp(cmd, "ls") == 0) cmd_ls(args);
        else if (strcmp(cmd, "cd") == 0) cmd_cd(args);
        else if (strcmp(cmd, "pwd") == 0) printf("%s\n", cwd_path);
        else if (strcmp(cmd, "mkdir") == 0) cmd_mkdir(args);
        else if (strcmp(cmd, "rmdir") == 0) cmd_rmdir(args);
        else printf("Unknown command: %s (type 'help' for available commands)\n", cmd);
    }
}