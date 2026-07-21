/* $OpenBSD: fuse.h,v 1.16 2026/01/29 06:04:27 helg Exp $ */
/*
 * Copyright (c) 2013 Sylvestre Gallon <ccna.syl@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _FUSE_H_
#define _FUSE_H_

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 26
#endif

#include <sys/stat.h>
#include <sys/statvfs.h>

#include <fcntl.h>
#include <utime.h>

#include "fuse_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct fuse_context {
	struct fuse *fuse;
	uid_t	     uid;
	gid_t	     gid;
	pid_t	     pid;
	void	     *private_data;
	mode_t	     umask;
};

struct fuse_config {
	int set_gid;		/* override gid for all files and directories */
	gid_t gid;
	int set_uid;		/* override uid for all files and directories */
	uid_t uid;
	int set_mode;
	mode_t dmask;		/* override mode for directories */
	mode_t fmask;		/* override mode for files */
	mode_t umask;		/* same as above if they are not set */
	int use_ino;		/* use the fs internal ino, not libfuse ino */
	int nullpath_ok;	/* some file ops will use fh only */

	/* Unsupported but included for compatibility */
	int kernel_cache;
	double entry_timeout;
	double negative_timeout;
	double attr_timeout;

	/* Never checked, always true */
	int readdir_ino;	/* return libfuse nodeid in readir */
	int hard_remove;	/* delete files immediately, even open ones */
};

/**
 * Readdir flags, passed to ->readdir()
 */
enum fuse_readdir_flags {
	FUSE_READDIR_DEFAULTS = 0,

	/* Unsupported */
	FUSE_READDIR_PLUS = (1 << 0)
};

/**
 * Readdir flags, passed to fuse_fill_dir_t callback.
 */
enum fuse_fill_dir_flags {
	FUSE_FILL_DIR_DEFAULTS = 0,

	/* Unsupported */
	FUSE_FILL_DIR_PLUS = (1 << 1)
};

typedef int (*fuse_fill_dir_t)(void *, const char *, const struct stat *,
    off_t, enum fuse_fill_dir_flags);

/*
 * Fuse operations work in the same way as their UNIX file system
 * counterparts. A major exception is that these routines return
 * a negated errno value (-errno) on failure.
 */
struct fuse_operations {
	int	(*getattr)(const char *, struct stat *,
		struct fuse_file_info *);
	int	(*readlink)(const char *, char *, size_t);
	int	(*mknod)(const char *, mode_t, dev_t);
	int	(*mkdir)(const char *, mode_t);
	int	(*unlink)(const char *);
	int	(*rmdir)(const char *);
	int	(*symlink)(const char *, const char *);
	int	(*rename)(const char *, const char *, unsigned int);
	int	(*link)(const char *, const char *);
	int	(*chmod)(const char *, mode_t, struct fuse_file_info *);
	int	(*chown)(const char *, uid_t, gid_t, struct fuse_file_info *);
	int	(*truncate)(const char *, off_t, struct fuse_file_info *);
	int	(*open)(const char *, struct fuse_file_info *);
	int	(*read)(const char *, char *, size_t, off_t,
		struct fuse_file_info *);
	int	(*write)(const char *, const char *, size_t, off_t,
		struct fuse_file_info *);
	int	(*statfs)(const char *, struct statvfs *);
	int	(*flush)(const char *, struct fuse_file_info *);
	int	(*release)(const char *, struct fuse_file_info *);
	int	(*fsync)(const char *, int, struct fuse_file_info *);
	/* Unsupported */
	int	(*setxattr)(const char *, const char *, const char *, size_t,
		int);
	/* Unsupported */
	int	(*getxattr)(const char *, const char *, char *, size_t);
	/* Unsupported */
	int	(*listxattr)(const char *, char *, size_t);
	/* Unsupported */
	int	(*removexattr)(const char *, const char *);
	int	(*opendir)(const char *, struct fuse_file_info *);
	int	(*readdir)(const char *, void *, fuse_fill_dir_t, off_t,
		struct fuse_file_info *, enum fuse_readdir_flags);
	int	(*releasedir)(const char *, struct fuse_file_info *);
	/* Unsupported */
	int	(*fsyncdir)(const char *, int, struct fuse_file_info *);
	void	*(*init)(struct fuse_conn_info *, struct fuse_config *);
	void	(*destroy)(void *);
	/* Unsupported */
	int	(*access)(const char *, int);
	/* Unsupported */
	int	(*create)(const char *, mode_t, struct fuse_file_info *);
	/* Unsupported */
	int	(*lock)(const char *, struct fuse_file_info *, int,
		struct flock *);
	int	(*utimens)(const char *, const struct timespec *,
		struct fuse_file_info *);
	/* Unsupported */
	int	(*bmap)(const char *, size_t, uint64_t *);
};

/*
 * API prototypes
 */
int fuse_mount(const struct fuse *, const char *);
void fuse_unmount(const struct fuse *);
int fuse_main(int, char **, const struct fuse_operations *, void *);
struct fuse *fuse_new(struct fuse_args *, const struct fuse_operations *,
    size_t, void *);
struct fuse_session *fuse_get_session(const struct fuse *);
struct fuse_context *fuse_get_context(void);
int fuse_loop(struct fuse *);
int fuse_loop_mt(struct fuse *, int);
void fuse_destroy(struct fuse *);
void fuse_lib_help(struct fuse_args *);

#ifdef __cplusplus
}
#endif

#endif /* _FUSE_H_ */
