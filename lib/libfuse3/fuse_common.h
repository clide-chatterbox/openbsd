/* $OpenBSD: fuse_common.h,v 1.1 2025/12/08 06:37:04 helg Exp $ */
/*
 * Copyright (c) 2026 Helg Bredow <helg@openbsd.org>
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

/*
 * This file contains definitions common to both the low and high-level FUSE
 * APIs.
 */

#if !defined(_FUSE_H_) && !defined(_FUSE_LOWLEVEL_H_)
#error "Never include <fuse_common.h> directly; use <fuse.h> or <fuse_lowlevel.h> instead."
#endif

#ifndef _FUSE_COMMON_H_
#define _FUSE_COMMON_H_

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

#include "fuse_opt.h"
#include "fuse_log.h"

#define FUSE_MAJOR_VERSION 3
#define FUSE_MINOR_VERSION 12
#define FUSE_MAKE_VERSION(maj, min)  ((maj) * 100 + (min))
#define FUSE_VERSION FUSE_MAKE_VERSION(FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION)

#ifdef __cplusplus
extern "C" {
#endif

struct fuse_file_info {
	int32_t		flags;			/* open(2) flags */
	uint32_t	direct_io	: 1;	/* only for compatibility */
	uint32_t	keep_cache	: 1;	/* only for compatibility */
	uint32_t	flush		: 1;	/* set on FUSE_FLUSH */
	uint32_t	padding		:29;
	uint64_t	fh;			/* file handle */
};

/* unsupported but needed for compilation of some ports */
#define FUSE_CAP_ASYNC_READ	 (1 << 0)
#define FUSE_CAP_POSIX_LOCKS     (1 << 1)
#define FUSE_CAP_ATOMIC_O_TRUNC  (1 << 3)
#define FUSE_CAP_EXPORT_SUPPORT  (1 << 4)
#define FUSE_CAP_DONT_MASK       (1 << 6)
#define FUSE_CAP_SPLICE_WRITE    (1 << 7)
#define FUSE_CAP_SPLICE_MOVE     (1 << 8)
#define FUSE_CAP_SPLICE_READ     (1 << 9)
#define FUSE_CAP_FLOCK_LOCKS     (1 << 10)
#define FUSE_CAP_IOCTL_DIR       (1 << 11)

struct fuse_conn_info {
	uint32_t	proto_major;
	uint32_t	proto_minor;
	uint32_t	max_write;

	/* only for compatibility */
	uint32_t	max_read;
	uint32_t	max_readahead;
	uint32_t	capable;
	uint32_t	want;
	uint32_t	max_background;
	uint32_t	congestion_threshold;
	uint32_t	time_gran;
	uint32_t	max_backing_stack_depth;
	uint32_t	no_interrupt	:1;
	uint32_t	padding		:31;
	uint64_t	capable_ext;
	uint64_t	want_ext;
	uint16_t	request_timeout;
};

struct fuse_session;

/*
 * API prototypes
 */
int fuse_version(void);
const char *fuse_pkgversion(void);
int fuse_daemonize(int);
int fuse_set_signal_handlers(struct fuse_session *);
void fuse_remove_signal_handlers(struct fuse_session *);

/*
 * Single data buffer
 */
struct fuse_buf {
	size_t size;		/* Size of data in bytes */
	void *mem;		/* Memory pointer to kernel fusebuf */
	size_t mem_size;	/* Used only if mem was internally allocated */
};

/*
 * Data buffer vector
 * TODO Always has only one element for now.
 */
struct fuse_bufvec {
	size_t count;
	size_t idx;
	size_t off;
	struct fuse_buf buf[1];
};

#if defined(__clang__) || __GNUC_PREREQ__(4, 6)
#define __warning(args)		__attribute__ ((warning( args )))
#else
#define __warning(args)		/* delete */
#endif

/*
 * Unsupported API to configure multi-threaded session loop.
 */
struct fuse_loop_config {
	int clone_fd;
	unsigned int max_threads;
};
struct fuse_loop_config *fuse_loop_cfg_create(void)
    __warning("This function is unsupported on OpenBSD");
void fuse_loop_cfg_destroy(struct fuse_loop_config *)
    __warning("This function is unsupported on OpenBSD");
void fuse_loop_cfg_set_max_threads(struct fuse_loop_config *, unsigned int)
    __warning("This function is unsupported on OpenBSD");
void fuse_loop_cfg_set_clone_fd(struct fuse_loop_config *, unsigned int)
    __warning("This function is unsupported on OpenBSD");

/* Unsupported API to set want_ext field of fuse_conn_info. */
bool fuse_set_feature_flag(struct fuse_conn_info *, uint64_t)
    __warning("This function is unsupported on OpenBSD");
void fuse_unset_feature_flag(struct fuse_conn_info *, uint64_t)
    __warning("This function is unsupported on OpenBSD");
bool fuse_get_feature_flag(const struct fuse_conn_info *, uint64_t)
    __warning("This function is unsupported on OpenBSD");

#if !defined(FUSE_USE_VERSION) || FUSE_USE_VERSION < 30
#  error only API version 30 or greater is supported
#endif

#ifdef __cplusplus
}
#endif

#endif /* _FUSE_COMMON_H_ */
