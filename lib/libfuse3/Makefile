#	$OpenBSD: Makefile,v 1.20 2026/02/02 09:17:06 helg Exp $

LIB=	fuse3

CFLAGS+=	-Wall -Wshadow -Wmissing-prototypes
CFLAGS+=	-Wstrict-prototypes -Wsign-compare

CDIAGFLAGS+=	-Wno-long-long -Wall -W -Wnested-externs -Wformat=2
CDIAGFLAGS+=	-Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
CDIAGFLAGS+=	-Wwrite-strings -Wshadow -Wpointer-arith -Wsign-compare
CDIAGFLAGS+=	-Wundef -Wbad-function-cast -Winline -Wcast-align

CFLAGS+=	-I${.CURDIR}
CFLAGS+=        -DFUSE_USE_VERSION=312

SRCS=	fuse_subr.c fuse_session.c fuse_lowlevel.c fuse_log.c fuse.c fuse_ops.c
HDRS=	fuse.h fuse_common.h fuse_lowlevel.h fuse_log.h

# Shared with libfuse
# WARNING: some of these files include fuse_private.h which is picked up from
# libfuse
.PATH:	${.CURDIR}/../libfuse
CFLAGS+=	-I${.CURDIR}/../libfuse
SRCS+=	dict.c tree.c fuse_opt.c
HDRS+=	${.CURDIR}/../libfuse/fuse_opt.h

VERSION_SCRIPT=	${.CURDIR}/Symbols.map

PC_FILES=fuse3.pc
CLEANFILES+=${PC_FILES}

includes:
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${DESTDIR}/usr/include/fuse3
	@cd ${.CURDIR}; for i in ${HDRS}; do \
		j="cmp -s $$i ${DESTDIR}/usr/include/fuse3/$$i || \
		    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	            $$i ${DESTDIR}/usr/include/fuse3"; \
	        echo $$j; \
	        eval "$$j"; \
	done

all: ${PC_FILES}
${PC_FILES}: fuse_private.h
	/bin/sh ${.CURDIR}/generate_pkgconfig.sh -c ${.CURDIR} -o ${.OBJDIR}

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o root -g ${SHAREGRP} \
	    -m ${SHAREMODE} ${.OBJDIR}/${PC_FILES} ${DESTDIR}/usr/lib/pkgconfig/

.include <bsd.lib.mk>
