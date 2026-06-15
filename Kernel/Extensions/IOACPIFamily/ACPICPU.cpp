/*
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc. All Rights
 * Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * This file was modified by William Kent in 2017 to support the PureDarwin
 * project. This notice is included in support of clause 2.2(b) of the License.
 */

#include "ACPICPU.h"

#undef super
#define super IOCPU

uint32_t ncpus = 0;

OSDefineMetaClassAndStructors(ACPICPU, IOCPU);

IOService *ACPICPU::probe(IOService *provider, SInt32 *score) {
	return this;
}

bool ACPICPU::startCommon() {
	if (startCommonCompleted) return true;

	cpuIC = new ACPICPUInterruptController;
	if (cpuIC == 0) return false;
	if (cpuIC->initCPUInterruptController(ncpus) != kIOReturnSuccess) return false;

	cpuIC->attach(this);
	cpuIC->registerCPUInterruptController();

	setCPUState(kIOCPUStateUninitalized);
	initCPU(true);
	registerService();

	startCommonCompleted = true;
	return true;
}

bool ACPICPU::start(IOService *provider) {
        kprintf("ACPICPU::start(%p)\n", provider);
        const OSSymbol * cpuNumSym = OSSymbol::withCStringNoCopy("cpu-number");
        OSNumber *cpuNum = OSDynamicCast(OSNumber, getProperty(cpuNumSym));
        cpuNumSym->release();
        if (cpuNum)
            index = cpuNum->unsigned32BitValue();

	if (!super::start(provider)) return false;
	return startCommon();
}

void ACPICPU::initCPU(bool boot) {
	cpuIC->enableCPUInterrupt(this);
	setCPUState(kIOCPUStateRunning);
}

void ACPICPU::quiesceCPU() {
	// Not required.
}

kern_return_t ACPICPU::startCPU(vm_offset_t start_paddr, vm_offset_t arg_paddr) {
	// Not implemented.
	kprintf("ACPICPU::startCPU(%p, %p) is a stub! (KERN_FAILURE)\n",
	    (void*)start_paddr, (void*)arg_paddr);
	return KERN_FAILURE;
}

void ACPICPU::haltCPU() {
	// Not required.
}

const OSSymbol *ACPICPU::getCPUName() {
        char buf[32];
        snprintf(buf, sizeof(buf), "cpu%u", index);
	return OSSymbol::withCString(buf);
}

#pragma mark -
#undef super
#define super IOCPUInterruptController

OSDefineMetaClassAndStructors(ACPICPUInterruptController, IOCPUInterruptController);

// ravynOS second-level interrupt controller dispatch table. Populated via
// ACPIRegisterSecondLevelIC() (called by IOPCIFamily for the MSI controller).
// Read at interrupt time, so keep it small and lock-free; entries are only ever
// appended during single-threaded platform init.
struct ACPISecondLevelIC {
	uint32_t                base;
	uint32_t                count;
	IOInterruptController * controller;
};
#define kACPIMaxSecondLevelICs 4
static ACPISecondLevelIC gACPISecondLevelICs[kACPIMaxSecondLevelICs];
static volatile uint32_t gACPINumSecondLevelICs = 0;

void ACPIRegisterSecondLevelIC(uint32_t base, uint32_t count,
                               IOInterruptController *ic) {
	if (!ic) return;
	uint32_t i = gACPINumSecondLevelICs;
	if (i >= kACPIMaxSecondLevelICs) return;
	gACPISecondLevelICs[i].base       = base;
	gACPISecondLevelICs[i].count      = count;
	gACPISecondLevelICs[i].controller = ic;
	__sync_synchronize();
	gACPINumSecondLevelICs = i + 1;
}

IOReturn ACPICPUInterruptController::handleInterrupt(void *refCon, IOService *nub, int source) {
	// Override the implementation in IOCPUInterruptController to
	// dispatch interrupts the old way. The source argument is ignored;
	// the first IOCPUInterruptController in the vector array is always used.

	// ravynOS: route a fired IDT vector to a registered second-level interrupt
	// controller (e.g. the PCI MSI/MSI-X controller) if it owns the vector.
	uint32_t n = gACPINumSecondLevelICs;
	for (uint32_t i = 0; i < n; i++) {
		uint32_t base  = gACPISecondLevelICs[i].base;
		uint32_t count = gACPISecondLevelICs[i].count;
		if ((uint32_t) source >= base && (uint32_t) source < base + count) {
			return gACPISecondLevelICs[i].controller->handleInterrupt(refCon, nub, source);
		}
	}

	IOInterruptVector *vector = &vectors[0];
	if (!vector->interruptRegistered) return kIOReturnInvalid;

	vector->handler(vector->target, refCon, vector->nub, source);
	return kIOReturnSuccess;
}
