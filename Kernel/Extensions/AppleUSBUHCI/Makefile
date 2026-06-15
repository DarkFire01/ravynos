MK_WERROR = no
KEXT = AppleUSBUHCI
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = com.apple.driver.AppleUSBUHCI
BUNDLE_VERSION = 560.4.2
KERNEL_PRIVATE = yes

SRCS = \
	AppleUHCIListElement.cpp \
	AppleUHCIqhMemoryBlock.cpp \
	AppleUHCItdMemoryBlock.cpp \
	AppleUSBUHCI.cpp \
	AppleUSBUHCI_Interrupts.cpp \
	AppleUSBUHCI_Obsolete.cpp \
	AppleUSBUHCI_PwrMgmt.cpp \
	AppleUSBUHCI_RootHub.cpp \
	AppleUSBUHCI_UIM.cpp

CFLAGS = -DPRIVATE=1 -DXNU_KERNEL_PRIVATE \
	-include ${.CURDIR}/usb_prefix.h \
	-Wno-implicit-int-conversion -Wno-inconsistent-missing-override \
	-Wno-sign-conversion -Wno-shadow -Wno-cast-qual -Wno-format -Wno-c++11-narrowing \
	-I${.CURDIR} \
	-I${.CURDIR:H}/AppleUSBEHCI \
	-I${.CURDIR:H}/AppleUSBOHCI \
	-I${.CURDIR:H}/AppleUSBUHCI \
	-I${.CURDIR:H}/IOUSBFamily \
	-I${.CURDIR:H}/IOUSBFamily/IOKit/usb \
	-I${.CURDIR:H}/IOPCIFamily \
	-I${.CURDIR:H}/IOACPIFamily/include \
	-I${.CURDIR}/../../xnu/osfmk \
	-I${.CURDIR}/../../xnu/bsd \
	-I${.CURDIR}/../../xnu/iokit \
	-I${.CURDIR}/../../xnu/libkern \
	-I${.CURDIR}/../../xnu \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/libkern \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/EXTERNAL_HEADERS

CXXSTD = gnu++11
CSTD = gnu11

.include <rvn.kext.mk>
