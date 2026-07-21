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

#ifndef FUSE_LOG_H_
#define FUSE_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Log severity levels that map to syslog levels.
 * Logging to syslog is not implemented yet so these do nothing.
 */
enum fuse_log_level {
	FUSE_LOG_EMERG,
	FUSE_LOG_ALERT,
	FUSE_LOG_CRIT,
	FUSE_LOG_ERR,
	FUSE_LOG_WARNING,
	FUSE_LOG_NOTICE,
	FUSE_LOG_INFO,
	FUSE_LOG_DEBUG
};

void fuse_log(enum fuse_log_level, const char *, ...);

#ifdef __cplusplus
}
#endif

#endif /* FUSE_LOG_H_ */
