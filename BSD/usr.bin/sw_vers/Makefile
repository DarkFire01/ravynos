PROG = sw_vers
SRCS = sw_vers.c
MAN =
MK_WERROR = no

CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${ROOT_SOURCE_DIR}/Frameworks \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern

LDFLAGS = -framework CoreFoundation -Wl,-platform_version -Wl,macos \
	  -Wl,${MACOS_VERSION_MIN} -Wl,${MACOS_VERSION_MIN} \
	  -Wl,-syslibroot,${RAVYN_SDKROOT}/usr/lib

# Let bsd.prog.mk build ${PROG} normally (it strips ${PROG}.full -> ${PROG} when
# MK_DEBUG_FILES != no). Don't redefine ${PROG}'s recipe -- doing so overrode the
# strip step and left the binary unbuilt. Copy the built binary into the sysroot
# via a separate target that depends on the real ${PROG}.
all: ${SYSROOT_DIR}/usr/bin/${PROG}

${SYSROOT_DIR}/usr/bin/${PROG}: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	cp -f ${.OBJDIR}/${PROG} ${SYSROOT_DIR}/usr/bin/

install: ${PROG}

.include <bsd.prog.mk>
