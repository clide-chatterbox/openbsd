/* $OpenBSD: fuse.c,v 1.60 2026/06/17 13:29:01 helg Exp $ */
/*
 * Copyright (c) 2013 Sylvestre Gallon <ccna.syl@gmail.com>
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

#include <sys/uio.h>

#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "fuse_private.h"

static struct fuse_context *ictx = NULL;

void
fuse_cmdline_help(void)
{
	printf(
	    "    -h   --help            show this help\n"
	    "    -V   --version         print fuse version\n"
	    "    -d   -o debug          enable debug output (implies -f)\n"
	    "    -f                     run in foreground\n"
	    "    -s                     use a single thread (always true)\n"
	);
}

#define CMDLINE_OPT(t, p) { t, offsetof(struct fuse_cmdline_opts, p), 1 }

/* options supported by fuse_parse_cmdline */
static struct fuse_opt cmdline_opts[] = {
	CMDLINE_OPT("-h",			show_help),
	CMDLINE_OPT("--help",			show_help),
	CMDLINE_OPT("-V",			show_version),
	CMDLINE_OPT("--version",		show_version),
	CMDLINE_OPT("-f",			foreground),
	CMDLINE_OPT("-s",			singlethread),
	CMDLINE_OPT("-d",			debug),
	CMDLINE_OPT("debug",			debug),
	CMDLINE_OPT("-d",			foreground),
	CMDLINE_OPT("debug",			foreground),
	CMDLINE_OPT("max_threads=%u",		max_threads),
	FUSE_OPT_KEY("-d",			FUSE_OPT_KEY_KEEP),
	FUSE_OPT_KEY("debug",			FUSE_OPT_KEY_KEEP),
	FUSE_OPT_END
};

/* options supported by fuse_new */
#define FUSE_LIB_OPT(o, m) {o, offsetof(struct fuse_config, m), 1}
static struct fuse_opt fuse_lib_opts[] = {
	FUSE_OPT_KEY("-d",			FUSE_OPT_KEY_KEEP),
	FUSE_OPT_KEY("debug",			FUSE_OPT_KEY_KEEP),
	FUSE_LIB_OPT("gid=",			set_gid),
	FUSE_LIB_OPT("gid=%u",			gid),
	FUSE_LIB_OPT("uid=",			set_uid),
	FUSE_LIB_OPT("uid=%u",			uid),
	FUSE_LIB_OPT("use_ino",			use_ino),
	FUSE_LIB_OPT("umask=",			set_mode),
	FUSE_LIB_OPT("umask=%o",		umask),
	FUSE_LIB_OPT("dmask=",			set_mode),
	FUSE_LIB_OPT("dmask=%o",		dmask),
	FUSE_LIB_OPT("fmask=",			set_mode),
	FUSE_LIB_OPT("fmask=%o",		fmask),
	FUSE_LIB_OPT("kernel_cache",		kernel_cache),
	FUSE_LIB_OPT("entry_timeout=",		entry_timeout),
	FUSE_LIB_OPT("negative_timeout=",	negative_timeout),
	FUSE_LIB_OPT("attr_timeout=",		attr_timeout),
	FUSE_OPT_END
};

extern struct fuse_lowlevel_ops llops;

int
fuse_loop(struct fuse *fuse)
{
	return (fuse_session_loop(fuse_get_session(fuse)));
}
DEF(fuse_loop);

int
fuse_loop_mt(struct fuse *fuse, int clone_fd)
{
	return (fuse_session_loop_mt(fuse_get_session(fuse), NULL));
}
DEF(fuse_loop_mt);

int
fuse_mount(const struct fuse *fuse, const char *dir)
{
	return (fuse_session_mount(fuse_get_session(fuse), dir));
}
DEF(fuse_mount);

void
fuse_unmount(const struct fuse *fuse)
{
	fuse_session_unmount(fuse_get_session(fuse));
}
DEF(fuse_unmount);

struct fuse_session *
fuse_get_session(const struct fuse *f)
{
	return (f->se);
}
DEF(fuse_get_session);

struct fuse *
fuse_new(struct fuse_args *args, const struct fuse_operations *ops,
    unused size_t size, void *userdata)
{
	struct fuse *fuse;
	struct fuse_vnode *root;

	if ((fuse = calloc(1, sizeof(*fuse))) == NULL)
		return (NULL);

	/* copy fuse ops to their own structure */
	memcpy(&fuse->op, ops, sizeof(fuse->op));

	if (fuse_opt_parse(args, &fuse->conf, fuse_lib_opts, NULL) == -1) {
		free(fuse);
		return (NULL);
	}

	fuse->max_ino = FUSE_ROOT_INO;
	fuse->private_data = userdata;
	fuse->se = fuse_session_new(args, &llops, sizeof(llops), fuse);
	if (fuse->se == NULL) {
		free(fuse);
		return (NULL);
	}

	if ((root = alloc_vn(fuse, "/", FUSE_ROOT_INO, 0)) == NULL) {
		free(fuse);
		return (NULL);
	}

	tree_init(&fuse->vnode_tree);
	tree_init(&fuse->name_tree);
	if (!set_vn(fuse, root)) {
		free(fuse);
		return (NULL);
	}

	/*
	 * Prepare the context that is available to file system operations via
	 * fuse_get_context(3). The pid, gid, uid and umask fields are set
	 * on demand when this is called in a requeset handle.
	 */
	ictx = calloc(1, sizeof(*ictx));
	if (ictx == NULL) {
		free(fuse);
		return (NULL);
	}

	ictx->fuse = fuse;
	ictx->private_data = userdata;

	if (ops->setxattr)
		LOG_WARNING("libfuse: setxattr not supported\n");
	if (ops->getxattr)
		LOG_WARNING("libfuse: getxattr not supported\n");
	if (ops->listxattr)
		LOG_WARNING("libfuse: listxattr not supported\n");
	if (ops->removexattr)
		LOG_WARNING("libfuse: removexattr not supported\n");
	if (ops->fsyncdir)
		LOG_WARNING("libfuse: fsyncdir not supported\n");
	if (ops->access)
		LOG_WARNING("libfuse: access not supported\n");
	if (ops->create)
		LOG_WARNING("libfuse: create not supported\n");
	if (ops->lock)
		LOG_WARNING("libfuse: lock not supported\n");
	if (ops->bmap)
		LOG_WARNING("libfuse: bmap not supported\n");

	return (fuse);
}
DEF(fuse_new);

int
fuse_daemonize(int foreground)
{
	if (foreground)
		return (0);

	return (daemon(0, 0));
}
DEF(fuse_daemonize);

void
fuse_destroy(struct fuse *fuse)
{
	fuse_session_destroy(fuse_get_session(fuse));
	free(fuse);
	free(ictx);
	ictx = NULL;
}
DEF(fuse_destroy);

static void
ifuse_sig_handler(int signum)
{
	/* empty handler to dinstinguish between SIG_IGN */
}

void
fuse_remove_signal_handlers(unused struct fuse_session *se)
{
	struct sigaction old_sa;

	if (sigaction(SIGHUP, NULL, &old_sa) == 0)
		if (old_sa.sa_handler == ifuse_sig_handler)
			signal(SIGHUP, SIG_DFL);

	if (sigaction(SIGINT, NULL, &old_sa) == 0)
		if (old_sa.sa_handler == ifuse_sig_handler)
			signal(SIGINT, SIG_DFL);

	if (sigaction(SIGTERM, NULL, &old_sa) == 0)
		if (old_sa.sa_handler == ifuse_sig_handler)
			signal(SIGTERM, SIG_DFL);

	if (sigaction(SIGPIPE, NULL, &old_sa) == 0)
		if (old_sa.sa_handler == SIG_IGN)
			signal(SIGPIPE, SIG_DFL);
}
DEF(fuse_remove_signal_handlers);

int
fuse_set_signal_handlers(unused struct fuse_session *se)
{
	struct sigaction old_sa;

	if (sigaction(SIGHUP, NULL, &old_sa) == -1)
		return (-1);
	if (old_sa.sa_handler == SIG_DFL)
		signal(SIGHUP, ifuse_sig_handler);

	if (sigaction(SIGINT, NULL, &old_sa) == -1)
		return (-1);
	if (old_sa.sa_handler == SIG_DFL)
		signal(SIGINT, ifuse_sig_handler);

	if (sigaction(SIGTERM, NULL, &old_sa) == -1)
		return (-1);
	if (old_sa.sa_handler == SIG_DFL)
		signal(SIGTERM, ifuse_sig_handler);

	if (sigaction(SIGPIPE, NULL, &old_sa) == -1)
		return (-1);
	if (old_sa.sa_handler == SIG_DFL)
		signal(SIGPIPE, SIG_IGN);

	return (0);
}
DEF(fuse_set_signal_handlers);

void
fuse_lib_help(struct fuse_args *fargs)
{
	printf(
	    "    -o umask=M             set file permissions (octal)\n"
	    "    -o uid=N               set file owner\n"
	    "    -o gid=N               set file group\n"
	);
	fuse_lowlevel_help();
}

static int
ifuse_process_opt(void *data, const char *arg, int key,
    unused struct fuse_args *args)
{
	struct fuse_cmdline_opts *opt = data;

	switch (key) {
	case FUSE_OPT_KEY_NONOPT:
		if (opt->mountpoint == NULL)
			opt->mountpoint = strdup(arg);
		else {
			fuse_log(FUSE_LOG_ERR, "libfuse: invalid argument %s\n",
				arg);
			return (-1);
		}

		return (0);
	}

	/* Pass through unknown options. */
	return (1);
}

int
fuse_parse_cmdline(struct fuse_args *args, struct fuse_cmdline_opts *opts)
{
	memset(opts, 0, sizeof(*opts));

	/* Only single-threaded is currently supported. */
	opts->singlethread = 1;

	if (fuse_opt_parse(args, opts, cmdline_opts, ifuse_process_opt) == -1)
		return (-1);

	return (0);
}
DEF(fuse_parse_cmdline);

struct fuse_context *
fuse_get_context(void)
{
	const fuse_req_t req = ifuse_req();
	const struct fuse_ctx *req_ctx = fuse_req_ctx(req);

	if (req_ctx == NULL) {
		ictx->uid = 0;
		ictx->gid = 0;
		ictx->pid = 0;
		ictx->umask = 0;
	} else {
		ictx->uid = req_ctx->uid;
		ictx->gid = req_ctx->gid;
		ictx->pid = req_ctx->pid;
		ictx->umask = req_ctx->umask;
	}

	return (ictx);
}
DEF(fuse_get_context);

int
fuse_version(void)
{
	return (FUSE_VERSION);
}
DEF(fuse_version);

const char *
fuse_pkgversion(void)
{
	return (FUSE_VERSION_PKG_INFO);
}
DEF(fuse_pkgversion);

/*
 * The following error codes may be returned from fuse_main():
 *   1: Invalid option arguments
 *   2: No mount point specified
 *   3: FUSE setup failed
 *   4: Mounting failed
 *   5: Failed to daemonize (detach from session)
 *   6: Failed to set up signal handlers
 *   7: An error occurred during the life of the file system
 */
int
fuse_main(int argc, char **argv, const struct fuse_operations *ops, void *data)
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_cmdline_opts opts;
	struct fuse *fuse;
	int ret = 0;

	if (fuse_parse_cmdline(&args, &opts))
		return (1);

	if (opts.show_version) {
		printf("FUSE library version %s\n", fuse_pkgversion());
		fuse_lowlevel_version();
		goto out1;
	}

	if (opts.show_help) {
		if(args.argv[0][0] != '\0')
			printf("usage: %s [options] <mountpoint>\n\n",
			       args.argv[0]);
		printf("FUSE options:\n");
		fuse_cmdline_help();
		fuse_lib_help(&args);
		goto out1;
	}

	if (!opts.show_help &&
	    !opts.mountpoint) {
		fuse_log(FUSE_LOG_ERR, "error: no mountpoint specified\n");
		ret = 2;
		goto out1;
	}

	fuse = fuse_new(&args, ops, sizeof(*ops), data);
	if (fuse == NULL) {
		ret = 3;
		goto out1;
	}

	if (fuse_mount(fuse, opts.mountpoint) != 0) {
		ret = 4;
		goto out2;
	}

	if (fuse_set_signal_handlers(fuse_get_session(fuse)) != 0) {
		ret = 6;
		goto out3;
	}

	fuse_daemonize(opts.foreground);

	if (fuse_loop(fuse))
		ret = 8;

	fuse_remove_signal_handlers(fuse_get_session(fuse));
out3:
	fuse_unmount(fuse);
out2:
	fuse_destroy(fuse);
out1:
	free(opts.mountpoint);
	fuse_opt_free_args(&args);
	return (ret);
}
DEF(fuse_main);

struct fuse_loop_config *
fuse_loop_cfg_create(void)
{
        struct fuse_loop_config *cfg;

	cfg = calloc(1, sizeof(*cfg));
        if (cfg == NULL)
                return NULL;

        cfg->max_threads = 10;

        return (cfg);
}
DEF(fuse_loop_cfg_create);

void
fuse_loop_cfg_destroy(struct fuse_loop_config *cfg)
{
	free(cfg);
}
DEF(fuse_loop_cfg_destroy);

void fuse_loop_cfg_set_max_threads(struct fuse_loop_config *cfg,
    unsigned int val)
{
	cfg->max_threads = val;
}
DEF(fuse_loop_cfg_set_max_threads);

void fuse_loop_cfg_set_clone_fd(struct fuse_loop_config *cfg, unsigned int val)
{
	cfg->clone_fd = val;
}
DEF(fuse_loop_cfg_set_clone_fd);
