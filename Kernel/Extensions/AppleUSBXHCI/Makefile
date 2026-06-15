MK_WERROR = no
KEXT = AppleUSBXHCI
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = com.apple.driver.AppleUSBXHCI
BUNDLE_VERSION = 560.4.2
KERNEL_PRIVATE = yes

# XHCI (USB3) host controller driver, from IOUSBFamily-560.4.2 (the last
# non-stripped tag). Matches qemu-xhci via IOPCIClassMatch 0x0c033000.
SRCS = AppleUSBXHCIUIM.cpp \
	AppleUSBXHCI_AsyncQueues.cpp \
	AppleUSBXHCI_IsocQueues.cpp \
	AppleUSBXHCI_RootHub.cpp \
	AppleUSBXHCI_Bandwidth.cpp \
	AppleUSBXHCI_PwrMgmt.cpp

CFLAGS = -DPRIVATE=1 -DXNU_KERNEL_PRIVATE \
	-include ${.CURDIR}/usb_prefix.h \
	-include ${.CURDIR}/xhci_ravyn_compat.h \
	-Wno-implicit-int-conversion \
	-Wno-inconsistent-missing-override \
	-Wno-sign-conversion -Wno-shadow \
	-Wno-cast-qual -Wno-format -Wno-c++11-narrowing \
	-I${.CURDIR} \
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
