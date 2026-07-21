/* $OpenBSD: fuse_session.c,v 1.2 2026/06/17 13:29:01 helg Exp $ */
/*
 * Copyright (c) 2025 Helg Bredow <helg@openbsd.org>
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

#include <sys/uio.h>
#include <sys/fusebuf.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "fuse_log.h"
#include "fuse_private.h"

#define DPRINTF(fmt, ...)						\
	do {								\
		if (se->debug)					\
			fuse_log(FUSE_LOG_DEBUG, fmt, ##__VA_ARGS__);	\
	} while(0)


#define DPERROR(s)							\
	do {								\
		if (se->debug)						\
			fuse_log(FUSE_LOG_DEBUG, "%s: %s", s,		\
			     strerror(errno));				\
	} while(0)

/* options supported by fuse_session_new */
#define FUSE_SESSION_OPT(o, m) {o, offsetof(struct fuse_session, m), 1}

static const struct fuse_opt fuse_ll_opts[] = {
	/* core options, also supported by fuse_parse_cmdline(3) */
	FUSE_SESSION_OPT("debug",	debug),
	FUSE_SESSION_OPT("-d",		debug),
	FUSE_SESSION_OPT("--debug",	debug),
	FUSE_OPT_END
};

/* options supported by fuse_session_mount */
#define FUSE_MOUNT_OPT(o, m) {o, offsetof(struct fuse_session, mnt_opts.m), 1}
static struct fuse_opt fuse_mnt_opts[] = {
	FUSE_MOUNT_OPT("allow_other",		allow_other),
	FUSE_MOUNT_OPT("default_permissions",	def_perms),
	FUSE_MOUNT_OPT("fsname=%s",		fsname),
	FUSE_MOUNT_OPT("max_read=%u",		max_read),
	FUSE_MOUNT_OPT("noatime",		noatime),
	FUSE_MOUNT_OPT("-r",			rdonly),
	FUSE_MOUNT_OPT("ro",			rdonly),
	FUSE_OPT_END
};

struct fuse_session *
fuse_session_new(struct fuse_args *fargs,
    const struct fuse_lowlevel_ops *llops, const size_t llops_len,
    void *userdata)
{
	struct fuse_session *se;
	int i;

	se = calloc(1, sizeof(*se));
	if (se == NULL)
		return (NULL);

	se->fci.proto_major = FUSE_KERNEL_VERSION;
	se->fci.proto_minor = FUSE_KERNEL_MINOR_VERSION;
	se->fci.max_write = FUSEBUFMAXSIZE;

	if (fuse_opt_parse(fargs, se, fuse_ll_opts, NULL) == -1)
		goto bad;

	if (fuse_opt_parse(fargs, se, fuse_mnt_opts, NULL) == -1)
		goto bad;

	/* check for unrecognised fargs */
	if (fargs->argc != 1) {
		LOG_ERR("libfuse: unkown option(s): ");
		for (i = 1; i < fargs->argc; i++)
			LOG_ERR("%s ", fargs->argv[i]);
		LOG_ERR("\n");

		goto bad;
	}

	/* validate size of ops struct */
	if (sizeof(se->llops) == llops_len)
		memcpy(&se->llops, llops, sizeof(se->llops));
	else
		goto bad;

	DPRINTF("FUSE library version %s\n", FUSE_VERSION_PKG_INFO);

	if (llops->fsyncdir)
		LOG_WARNING("libfuse: fsyncdir not supported\n");
	if (llops->setxattr)
		LOG_WARNING("libfuse: setxattr not supported\n");
	if (llops->getxattr)
		LOG_WARNING("libfuse: getxattr not supported\n");
	if (llops->listxattr)
		LOG_WARNING("libfuse: listxattr not supported\n");
	if (llops->removexattr)
		LOG_WARNING("libfuse: removexattr not supported\n");
	if (llops->access)
		LOG_WARNING("libfuse: access not supported\n");
	if (llops->create)
		LOG_WARNING("libfuse: create not supported\n");
	if (llops->bmap)
		LOG_WARNING("libfuse: bmap not supported\n");

	se->userdata = userdata;

	return (se);
bad:
	free(se);
	return (NULL);
}
DEF(fuse_session_new);

void
fuse_session_destroy(struct fuse_session *se)
{
	if (se->init && se->llops.destroy)
		se->llops.destroy(se->userdata);

	free(se);
}
DEF(fuse_session_destroy);

void
fuse_session_exit(struct fuse_session *se)
{
	se->exit = 1;
}
DEF(fuse_session_exit);

int
fuse_session_exited(struct fuse_session *se)
{
	return (se->exit);
}
DEF(fuse_session_exited);

void
fuse_session_reset(struct fuse_session *se)
{
	if (se != NULL)
		se->exit = 0;
}
DEF(fuse_session_reset);

int
fuse_session_loop(struct fuse_session *se)
{
	struct fuse_buf buf;
	int err;

	memset(&buf, 0, sizeof(buf));

	while (!fuse_session_exited(se)) {
		err = fuse_session_receive_buf(se, &buf);
		if (err == -EINTR || err == -ENODEV) {
			fuse_session_exit(se);
			continue;
		} else if (err <= 0) {
			DPERROR(__func__);
			break;
		}
		fuse_session_process_buf(se, &buf);
	}

	free(buf.mem);
	fuse_session_reset(se);

	return (err == 0 ? 0 : -1);
}
DEF(fuse_session_loop);

int
fuse_session_loop_mt(struct fuse_session *se, struct fuse_loop_config *cfg)
{
	if (cfg == NULL || cfg->max_threads != 1)
		LOG_WARNING("libfuse: no multi-threaded support, falling "
		    "back to single thread\n");

	return fuse_session_loop(se);
}
DEF(fuse_session_loop_mt);

static void
iprocess_init(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_conn_info *fci = &se->fci;
	struct fuse_init_out out;
	uint32_t major = fbuf->in.init.major;
	uint32_t minor = fbuf->in.init.minor;

	DPRINTF("%-11s", "init");
	DPRINTF("Kernel: %d.%d\t", major, minor);
	DPRINTF("libfuse: %d.%d\t", fci->proto_major, fci->proto_minor);

	if (major != fci->proto_major && minor != fci->proto_minor)
		errx(1, "libfuse: FUSE kernel protocol version mismatch");

	if (se->llops.init)
		se->llops.init(se->userdata, fci);

	memset(&out, 0, sizeof(out));
	out.major = fci->proto_major;
	out.minor = fci->proto_minor;
	out.max_write = fci->max_write;

	/* validate connection settings */
	if (se->fci.max_write > FUSEBUFMAXSIZE) {
		DPRINTF("libfuse: max_write %u too large, using %u instead.\n",
		    se->fci.max_write, FUSEBUFMAXSIZE);
		se->fci.max_write = FUSEBUFMAXSIZE;
	}

	DPRINTF("libfuse: conn: max_write=%u\t", out.max_write);
	DPRINTF("\n");

	fuse_reply_buf(req, (const char *)&out, sizeof(out));

	se->init = 1;
}

static void
iprocess_destroy(fuse_req_t req)
{
	struct fuse_session *se = req->se;

	DPRINTF("%-11s", "destroy");

	se->dead = 1;
	fuse_session_exit(se);
	fuse_reply_err(req, 0);
}

static void
iprocess_lookup(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *name = fb_dat(fbuf->hdr);

	DPRINTF("%-11s", "lookup");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);
	DPRINTF("name: %s\t", name);

	if (se->llops.lookup)
		se->llops.lookup(req, fbuf->fb_ino, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_getattr(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;

	DPRINTF("%-11s", "getattr");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);

	if (se->llops.getattr) {
		/* fuse_getattr_in is unused */
		se->llops.getattr(req, fbuf->fb_ino, NULL);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_setattr(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct stat stbuf;
	const int flags = fbuf->in.setattr.valid;

	DPRINTF("%-11s", "setattr");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);

	if (se->llops.setattr) {
		memset(&stbuf, 0, sizeof(stbuf));
		stbuf.st_size = fbuf->in.setattr.size;
		stbuf.st_atime = fbuf->in.setattr.atime;
		stbuf.st_mtime = fbuf->in.setattr.mtime;
		stbuf.st_atim.tv_nsec = fbuf->in.setattr.atimensec;
		stbuf.st_mtim.tv_nsec = fbuf->in.setattr.mtimensec;
		stbuf.st_mode = fbuf->in.setattr.mode;
		stbuf.st_uid = fbuf->in.setattr.uid;
		stbuf.st_gid = fbuf->in.setattr.gid;

		se->llops.setattr(req, fbuf->fb_ino, &stbuf, flags, NULL);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_opendir(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "opendir");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);

	memset(&ffi, 0, sizeof(ffi));
	ffi.flags = fbuf->in.open.flags;

	if (se->llops.opendir)
		se->llops.opendir(req, fbuf->fb_ino, &ffi);
	else
		fuse_reply_open(req, &ffi);
}

static void
iprocess_readdir(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "readdir");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);
	DPRINTF("fh: %llu\t", fbuf->in.read.fh);
	DPRINTF("size: %u\t", fbuf->in.read.size);
	DPRINTF("offset: %llu\t", fbuf->in.read.offset);

	if (se->llops.readdir) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->in.read.fh;

		se->llops.readdir(req, fbuf->fb_ino, fbuf->in.read.size,
		    fbuf->in.read.offset, &ffi);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_releasedir(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "releasedir");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);
	DPRINTF("fh: %llu\t", fbuf->in.read.fh);

	if (se->llops.releasedir) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->in.release.fh;
		ffi.flags = fbuf->in.release.flags;

		se->llops.releasedir(req, fbuf->fb_ino, &ffi);
	} else
		fuse_reply_err(req, 0);
}

static void
iprocess_mkdir(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *name = fb_dat(fbuf->in.mkdir);

	DPRINTF("%-11s", "mkdir");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);
	DPRINTF("mode: %#6o\t", fbuf->in.mkdir.mode);
	DPRINTF("name: %s\t", name);

	if (se->llops.mkdir) {
		req->ctx.umask = fbuf->in.mkdir.umask;
		se->llops.mkdir(req, fbuf->fb_ino, name, fbuf->in.mkdir.mode);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_rmdir(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *name = fb_dat(fbuf->hdr);

	DPRINTF("%-11s", "rmdir");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);
	DPRINTF("name: %s\t", name);

	if (se->llops.rmdir)
		se->llops.rmdir(req, fbuf->fb_ino, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_mknod(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *name = fb_dat(fbuf->in.mknod);

	DPRINTF("%-11s", "mknod");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);
	DPRINTF("mode: %#6o\t", fbuf->in.mknod.mode);
	DPRINTF("name: %s\t", name);

	if (se->llops.mknod) {
		req->ctx.umask = fbuf->in.mknod.umask;
		se->llops.mknod(req, fbuf->fb_ino, name, fbuf->in.mknod.mode,
		    fbuf->in.mknod.rdev);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_open(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "open");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);

	memset(&ffi, 0, sizeof(ffi));
	ffi.flags = fbuf->in.open.flags;

	if (se->llops.open)
		se->llops.open(req, fbuf->fb_ino, &ffi);
	else
		fuse_reply_open(req, &ffi);
}

static void
iprocess_read(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "read");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);
	DPRINTF("size: %u\t", fbuf->in.read.size);
	DPRINTF("offset: %llu\t", fbuf->in.read.offset);

	if (se->llops.read) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->in.read.fh;
		ffi.flags = fbuf->in.read.flags;

		se->llops.read(req, fbuf->fb_ino, fbuf->in.read.size,
		    fbuf->in.read.offset, &ffi);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_write(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "write");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);
	DPRINTF("size: %u\t", fbuf->in.write.size);
	DPRINTF("offset: %llu\t", fbuf->in.write.offset);

	if (se->llops.write) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->in.write.fh;

		se->llops.write(req, fbuf->fb_ino, fb_dat(fbuf->in.write),
		    fbuf->in.write.size, fbuf->in.write.offset, &ffi);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_fsync(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;
	const uint32_t fsync_flags = fbuf->in.fsync.fsync_flags & 1;

	DPRINTF("%-11s", "fsync");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);
	DPRINTF("flags: %u", fsync_flags);

	if (se->llops.fsync) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->in.fsync.fh;

		se->llops.fsync(req, fbuf->fb_ino, fsync_flags, &ffi);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_flush(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "flush");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);

	if (se->llops.flush) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->in.flush.fh;
		ffi.flush = 1;

		se->llops.flush(req, fbuf->fb_ino, &ffi);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_release(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "release");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);

	if (se->llops.release) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->in.release.fh;
		ffi.flags = fbuf->in.release.flags;

		se->llops.release(req, fbuf->fb_ino, &ffi);
	} else
		fuse_reply_err(req, 0);
}

static void
iprocess_forget(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const uint64_t nlookup = fbuf->in.forget.nlookup;

	DPRINTF("%-11s", "forget");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);
	DPRINTF("nlookup: %llu\t", nlookup);

	if (se->llops.forget)
		se->llops.forget(req, fbuf->fb_ino, nlookup);
	else
		fuse_reply_none(req);
}

static void
iprocess_symlink(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *target;
	const char *name;
	int len;

	name = fb_dat(fbuf->hdr);
	len = strlen(name);
	target = &name[len + 1];

	DPRINTF("%-11s", "symlink");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);
	DPRINTF("name: %s\t", name);
	DPRINTF("target: %s\t", target);

	if (se->llops.symlink)
		se->llops.symlink(req, target, fbuf->fb_ino, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_readlink(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;

	DPRINTF("%-11s", "readlink");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);

	if (se->llops.readlink)
		se->llops.readlink(req, fbuf->fb_ino);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_link(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *name = fb_dat(fbuf->in.link);
	const uint64_t oldnodeid = fbuf->in.link.oldnodeid;

	DPRINTF("%-11s", "link");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);
	DPRINTF("inode: %llu\t", oldnodeid);
	DPRINTF("name: %s\t", name);

	if (se->llops.link)
		se->llops.link(req, oldnodeid, fbuf->fb_ino, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_unlink(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *name = fb_dat(fbuf->hdr);

	DPRINTF("%-11s", "unlink");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);
	DPRINTF("name: %s\t", name);

	if (se->llops.unlink)
		se->llops.unlink(req, fbuf->fb_ino, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_rename(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const uint64_t newdir = fbuf->in.rename.newdir;
	const char *target;
	const char *name;
	int len;

	name = fb_dat(fbuf->in.rename);
	len = strlen(name);
	target = &name[len + 1];

	DPRINTF("%-11s", "rename");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);
	DPRINTF("name: %s\t", name);
	DPRINTF("newdir: %llu\t", newdir);
	DPRINTF("target: %s\t", target);

	if (se->llops.rename)
		se->llops.rename(req, fbuf->fb_ino, name, newdir, target, 0);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_statfs(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;

	DPRINTF("%-11s", "statfs");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("pid: %llu\t", fbuf->fb_tid);

	if (se->llops.statfs)
		se->llops.statfs(req, fbuf->fb_ino);
	else
		fuse_reply_err(req, ENOSYS);
}

int
fuse_session_receive_buf(struct fuse_session *se, struct fuse_buf *buf)
{
	ssize_t n;

	/*
	 * Prepare the read data buffer. We need enough space for the header,
	 * input struct and any additional data, filenames or the buffer for
	 * write(2). The minimum buffer size must be large enough for the
	 * name and path parameters for FUSE_SYMLINK.
	 */
	if (buf->mem == NULL) {
		if (se->fci.max_write > FUSEBUFMAXSIZE)
			buf->mem_size = sizeof(struct fusebuf) + FUSEBUFMAXSIZE;
		else if (se->fci.max_write < PATH_MAX + NAME_MAX)
			buf->mem_size = sizeof(struct fusebuf) + PATH_MAX +
			    NAME_MAX;
		else
			buf->mem_size = sizeof(struct fusebuf) +
			    se->fci.max_write;

		buf->mem = calloc(1, buf->mem_size);
		if (buf->mem == NULL) {
			DPERROR(__func__);
			return (-1);
		}
	}

	n = read(se->fd, buf->mem, buf->mem_size);
	if (n == -1)
		return (-errno);
	else
		buf->size = n;

	return (n);
}
DEF(fuse_session_receive_buf);

void
fuse_session_process_buf(struct fuse_session *se, const struct fuse_buf *buf)
{
	struct fusebuf *fbuf;
	struct fuse_req req;

	fbuf = (struct fusebuf *)buf->mem;
	req.fbuf = fbuf;
	req.se = se;
	req.ctx.uid = fbuf->fb_uid;
	req.ctx.gid = fbuf->fb_gid;
	req.ctx.pid = fbuf->fb_tid;

	/* later set in create, mknod, mkdir */
	req.ctx.umask = 0;

	/* need to at least have the header for the next check */
	if (buf->size < sizeof(fbuf->hdr))
		return;

	if (buf->size < fbuf->hdr.len)
		return;

	switch (fbuf->fb_type) {
	case FUSE_INIT:
		iprocess_init(&req);
		break;
	case FUSE_DESTROY:
		iprocess_destroy(&req);
		break;
	case FUSE_LOOKUP:
		iprocess_lookup(&req);
		break;
	case FUSE_GETATTR:
		iprocess_getattr(&req);
		break;
	case FUSE_SETATTR:
		iprocess_setattr(&req);
		break;
	case FUSE_OPENDIR:
		iprocess_opendir(&req);
		break;
	case FUSE_READDIR:
		iprocess_readdir(&req);
		break;
	case FUSE_RELEASEDIR:
		iprocess_releasedir(&req);
		break;
	case FUSE_MKDIR:
		iprocess_mkdir(&req);
		break;
	case FUSE_RMDIR:
		iprocess_rmdir(&req);
		break;
	case FUSE_MKNOD:
		iprocess_mknod(&req);
		break;
	case FUSE_OPEN:
		iprocess_open(&req);
		break;
	case FUSE_READ:
		iprocess_read(&req);
		break;
	case FUSE_WRITE:
		iprocess_write(&req);
		break;
	case FUSE_FSYNC:
		iprocess_fsync(&req);
		break;
	case FUSE_FLUSH:
		iprocess_flush(&req);
		break;
	case FUSE_RELEASE:
		iprocess_release(&req);
		break;
	case FUSE_FORGET:
		iprocess_forget(&req);
		break;
	case FUSE_SYMLINK:
		iprocess_symlink(&req);
		break;
	case FUSE_READLINK:
		iprocess_readlink(&req);
		break;
	case FUSE_LINK:
		iprocess_link(&req);
		break;
	case FUSE_UNLINK:
		iprocess_unlink(&req);
		break;
	case FUSE_RENAME:
		iprocess_rename(&req);
		break;
	case FUSE_STATFS:
		iprocess_statfs(&req);
		break;
	default:
		DPRINTF("Opcode: %i not supported\t", fbuf->fb_type);
		fuse_reply_err(&req, ENOSYS);
	}
	DPRINTF("\n");
}
DEF(fuse_session_process_buf);

int
fuse_session_fd(const struct fuse_session *se)
{
	if (se == NULL)
		return (-1);

	return (se->fd);
}
DEF(fuse_session_fd);

int
fuse_session_mount(struct fuse_session *se, const char *dir)
{
	struct fusefs_args fargs;
	const char *errcause;
	int mnt_flags;

	se->mnt_dir = NULL;

	if ((se->fd = open("/dev/fuse0", O_RDWR|O_CLOEXEC)) == -1) {
		perror("/dev/fuse0");
		goto bad;
	}

	mnt_flags = 0;
	if (se->mnt_opts.rdonly)
		mnt_flags |= MNT_RDONLY;
	if (se->mnt_opts.noatime)
		mnt_flags |= MNT_NOATIME;

	if (se->mnt_opts.max_read > FUSEBUFMAXSIZE) {
		LOG_ERR("libfuse: invalid max_read (%d > %d)\n",
		    se->mnt_opts.max_read, FUSEBUFMAXSIZE);
		goto bad;
	}

	memset(&fargs, 0, sizeof(fargs));
	fargs.fd = se->fd;
	fargs.max_read = se->mnt_opts.max_read;
	fargs.allow_other = se->mnt_opts.allow_other;

	if ((se->mnt_dir = realpath(dir, NULL)) == NULL) {
		LOG_ERR("libfuse: realpath %s: %s\n", dir, strerror(errno));
		goto bad;
	}

	if (mount(MOUNT_FUSEFS, se->mnt_dir, mnt_flags, &fargs)) {
		switch (errno) {
		case EMFILE:
			errcause = "mount table full";
			break;
		case EOPNOTSUPP:
			errcause = "filesystem not supported by kernel";
			break;
		default:
			errcause = strerror(errno);
			break;
		}
		LOG_ERR("libfuse: %s: %s\n", se->mnt_dir, errcause);
		goto bad;
	}

	if (se->debug) {
		/* OpenBSD FUSE kernel always default to this. */
		DPRINTF("libfuse: mount: default_permissions\n");

		if (se->mnt_opts.allow_other)
			DPRINTF("libfuse: mount: allow_other\n");
		if (se->mnt_opts.fsname)
			DPRINTF("libfuse: mount: fsname (unsupported): %s\n",
			     se->mnt_opts.fsname);
		if (se->mnt_opts.max_read)
			DPRINTF("libfuse: mount: max_read=%u\n",
			    se->mnt_opts.max_read);
		if (se->mnt_opts.noatime)
			DPRINTF("libfuse: mount: noatime\n");
		if (se->mnt_opts.rdonly)
			DPRINTF("libfuse: mount: read-only\n");
	}

	return (0);
bad:
	if (se->fd != -1)
		close(se->fd);
	free(se->mnt_dir);
	return (-1);
}
DEF(fuse_session_mount);

void
fuse_session_unmount(struct fuse_session *se)
{
	/*
	 * Close the device before unmounting to prevent deadlocks with
	 * FBT_DESTROY if fuse_loop() has already terminated.
	 */
	if (close(se->fd) == -1)
		DPERROR(__func__);
	se->fd = -1;

	if (!se->dead)
		if (unmount(se->mnt_dir, MNT_FORCE) == -1)
			DPERROR(__func__);

	free(se->mnt_dir);
	se->mnt_dir = NULL;
}
DEF(fuse_session_unmount);
