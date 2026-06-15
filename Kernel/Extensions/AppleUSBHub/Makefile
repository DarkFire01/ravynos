MK_WERROR = no
KEXT = AppleUSBHub
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = com.apple.driver.AppleUSBHub
BUNDLE_VERSION = 560.4.2
KERNEL_PRIVATE = yes

# USB hub driver (from IOUSBFamily-560.4.2). Matches IOUSBDevice class 9 (hub),
# including the controller's published IOUSBRootHubDevice; enumerates ports.
# AppleUSBHSHubUserClient.cpp omitted: EHCI high-speed-hub TT user client
# (pulls AppleUSBEHCI.h), not needed to drive an XHCI root hub.
SRCS = AppleUSBHub.cpp \
	AppleUSBHubPort.cpp \
	AppleUSBHub_ravyn_compat.cpp

CFLAGS = -DPRIVATE=1 -DXNU_KERNEL_PRIVATE \
	-include ${.CURDIR}/usb_prefix.h \
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
