#define _XOPEN_SOURCE 500 /* nftw, FTW_DEPTH, FTW_PHYS; CLOCK_MONOTONIC */
#define _DEFAULT_SOURCE   /* statvfs */

#include "motionsense/utils.h"

#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Time                                                                */
/* ------------------------------------------------------------------ */
long long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* ------------------------------------------------------------------ */
/* Filesystem / IO                                                     */
/* ------------------------------------------------------------------ */
int ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0)
        return S_ISDIR(st.st_mode) ? 0 : -1;
    if (mkdir(path, 0777) == 0)
        return 0;
    return -1;
}

int fsync_parent_dir(const char *path)
{
    char dir[160];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(dir))
        return -1;
    memcpy(dir, path, len + 1);
    char *slash = strrchr(dir, '/');
    if (!slash)
        return -1;
    if (slash == dir)
        slash[1] = '\0';
    else
        *slash = '\0';

    int fd = open(dir, O_RDONLY);
    if (fd < 0)
        return -1;
    int rc = fsync(fd);
    close(fd);
    return rc;
}

int write_all(int fd, const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    size_t off = 0;
    while (off < len)
    {
        ssize_t n = write(fd, p + off, len - off);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

static int nuke_cb(const char *path, const struct stat *st, int type, struct FTW *f)
{
    (void)st;
    (void)type;
    (void)f;
    return remove(path);
}

static int remove_recursive(const char *path)
{
    return nftw(path, nuke_cb, 16, FTW_DEPTH | FTW_PHYS);
}

int remove_path_tree(const char *path)
{
    struct stat st;
    if (lstat(path, &st) != 0)
    {
        return (errno == ENOENT) ? 0 : -1;
    }
    if (S_ISDIR(st.st_mode))
        return remove_recursive(path);
    return remove(path);
}

uint32_t fs_free_mb(const char *path)
{
    struct statvfs s;
    if (statvfs(path, &s) != 0)
        return UINT32_MAX;
    uint64_t free_bytes = (uint64_t)s.f_bavail * s.f_frsize;
    return (uint32_t)(free_bytes >> 20);
}
