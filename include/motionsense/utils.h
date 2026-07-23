#ifndef MOTIONSENSE_UTILS_H
#define MOTIONSENSE_UTILS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Round x up to the nearest multiple of 16. */
#define UPALIGNTO16(x) (((x) + 15) & ~15)

/* ------------------------------------------------------------------ */
/* Time                                                                */
/* ------------------------------------------------------------------ */

/* Milliseconds from a monotonic clock (CLOCK_MONOTONIC), immune to wall-clock
 * adjustments. Suitable for measuring intervals, not absolute time. */
long long now_ms(void);

/* ------------------------------------------------------------------ */
/* Filesystem / IO                                                     */
/* ------------------------------------------------------------------ */

/* Create `path` if it is missing. Returns 0 if it exists as a directory or was
 * created, -1 otherwise. */
int ensure_dir(const char *path);

/* fsync the directory that contains `path`, so a preceding create/rename is
 * durable across power loss. Returns 0 on success, -1 on error. */
int fsync_parent_dir(const char *path);

/* write() the whole buffer, retrying short writes and EINTR.
 * Returns 0 on success, -1 on error. */
int write_all(int fd, const void *buf, size_t len);

/* Recursively delete a file or directory tree (like "rm -rf", via nftw).
 * A missing path counts as success. Returns 0 on success, -1 on error. */
int remove_path_tree(const char *path);

/* Free space in MiB on the filesystem holding `path`.
 * Returns UINT32_MAX if statvfs fails. */
uint32_t fs_free_mb(const char *path);

#ifdef __cplusplus
}
#endif
#endif
