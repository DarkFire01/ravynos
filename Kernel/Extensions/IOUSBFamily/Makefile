MK_WERROR = no
KEXT = IOUSBFamily
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = com.apple.iokit.IOUSBFamily
BUNDLE_VERSION = 560.4.2
KERNEL_PRIVATE = yes

# Core IOUSBFamily classes (from apple-oss IOUSBFamily-560.4.2, the last
# non-stripped tag). Consumer kexts (AppleUSBXHCI, AppleUSBHub, ...) link
# against the symbols these provide.
SRCS = IOUSBController.cpp \
	IOUSBControllerV2.cpp \
	IOUSBControllerV3.cpp \
	IOUSBController_Pipes.cpp \
	IOUSBController_Errata.cpp \
	IOUSBControllerListElement.cpp \
	IOUSBControllerUserClient.cpp \
	IOUSBDevice.cpp \
	IOUSBInterface.cpp \
	IOUSBPipe.cpp \
	IOUSBPipeV2.cpp \
	IOUSBNub.cpp \
	IOUSBCommand.cpp \
	IOUSBWorkLoop.cpp \
	IOUSBHubDevice.cpp \
	IOUSBRootHubDevice.cpp \
	IOUSBHubPolicyMaker.cpp \
	IOUSBBus.cpp \
	IOUSBLog.cpp \
	IOUSBDeviceUserClient.cpp \
	IOUSBInterfaceUserClient.cpp \
	IOUSBInterfaceUserClientV3.cpp \
	IOUSBUserClientInit.cpp \
	IOUSBFamily_ravyn_compat.cpp

CFLAGS = -DPRIVATE=1 -DXNU_KERNEL_PRIVATE \
	-include ${.CURDIR}/usb_prefix.h \
	-I${.CURDIR}/IOKit/usb \
	-Wno-implicit-int-conversion \
	-Wno-inconsistent-missing-override \
	-Wno-sign-conversion -Wno-shadow \
	-Wno-cast-qual -Wno-format -Wno-c++11-narrowing \
	-I${.CURDIR} \
	-I${.CURDIR}/../../xnu/osfmk \
	-I${.CURDIR}/../../xnu/bsd \
	-I${.CURDIR}/../../xnu/iokit \
	-I${.CURDIR}/../../xnu/libkern \
	-I${.CURDIR}/../../xnu \
	-I${.CURDIR:H}/IOPCIFamily \
	-I${.CURDIR:H}/IOACPIFamily/include \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/libkern \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/EXTERNAL_HEADERS

CXXSTD = gnu++11
CSTD = gnu11

.include <rvn.kext.mk>
