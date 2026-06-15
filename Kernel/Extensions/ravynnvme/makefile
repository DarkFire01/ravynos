MK_WERROR = no
KEXT = ravynnvme
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = com.ravynos.driver.ravynnvme
BUNDLE_VERSION = 1.0.0

SRCS = RavynNVMe.cpp RavynNVMeDisk.cpp

SLF = /System/Library/Frameworks
CFLAGS = -Wno-implicit-int-conversion -Wno-sign-conversion \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${.CURDIR:H}/IOStorageFamily \
	-I${.CURDIR:H}/IOPCIFamily \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk

.include <rvn.kext.mk>
