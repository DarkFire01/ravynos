SUBDIR = AppleAPIC IOACPIFamily IOPCIFamily IOStorageFamily IOATAFamily \
	 AppleIntelPIIXATA msdosfs hfs AppleFileSystemDriver RavynAHCIPort \
	 IOGraphics corecrypto pthread #IOHIDFamily

all: kernelcache
.for target_dir in ${RAVYN_SDKROOT}/System/Library/Extensions \
  ${SYSROOT_DIR}/System/Library/Extensions
	mkdir -p ${target_dir}
.for kext in ${SUBDIR}
	cp -fR ${.OBJDIR}/${kext}/${kext:S/IOGraphics/IOGOPFramebuffer/}.kext ${target_dir}/
.endfor
.endfor

EXTENSIONS = ${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/Extensions
XNU_OBJDIR = ${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu

kernelcache: ${SUBDIR}
	@echo Generating prelinked kernel cache
	${TOOLS}/plktool ${.OBJDIR}/${.TARGET}.tmp \
	${XNU_OBJDIR}/sym/kernel \
	${XNU_OBJDIR}/sym/System.kext \
	${XNU_OBJDIR}/sym/System.kext/PlugIns/* \
	${EXTENSIONS}/IOACPIFamily/IOACPIFamily.kext \
	${EXTENSIONS}/AppleAPIC/AppleAPIC.kext \
	${EXTENSIONS}/IOPCIFamily/IOPCIFamily.kext \
	${EXTENSIONS}/corecrypto/corecrypto.kext \
	${EXTENSIONS}/pthread/pthread.kext \
	${EXTENSIONS}/IOATAFamily/IOATAFamily.kext \
	${EXTENSIONS}/IOStorageFamily/IOStorageFamily.kext \
	${EXTENSIONS}/RavynAHCIPort/RavynAHCIPort.kext \
	${EXTENSIONS}/AppleFileSystemDriver/AppleFileSystemDriver.kext \
	${EXTENSIONS}/hfs/hfs_encodings/hfs_encodings.kext \
	${EXTENSIONS}/hfs/hfs.kext \
	${EXTENSIONS}/IOGraphics/IOGOPFramebuffer.kext
	chmod 0644 ${.TARGET}.tmp
	# plktool already emits the final compressed-container ('comp') kernelcache,
	# which is not a Mach-O, so lipo cannot process it. For a single-arch build the
	# universal/fat wrapper is unnecessary -- the .tmp IS the bootable kernelcache.
	cp -f ${.OBJDIR}/${.TARGET}.tmp ${.OBJDIR}/${.TARGET}
	cp -f ${.OBJDIR}/${.TARGET} ${ROOT_BINARY_DIR}

.include <bsd.subdir.mk>
