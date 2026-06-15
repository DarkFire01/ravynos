/*
 * RavynNVMe: minimal driver for NVM Express (NVMe) storage controllers
 *
 * Copyright (C) 2026 ravynOS Project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/storage/IOMedia.h>
#include <libkern/OSAtomic.h>
#include <kern/clock.h>
#include "RavynNVMe.h"
#include "RavynNVMeDisk.h"

#define super IOService
OSDefineMetaClassAndStructors(RavynNVMe, IOService);

/* ---- Per-queue DMA layout (one physically-contiguous buffer each) ----- */
#define NVME_PAGE_SIZE      4096u
#define kSQOffset           0x0000u   /* submission queue (1 page) */
#define kCQOffset           0x1000u   /* completion queue (1 page) */
#define kAdminBounceOffset  0x2000u   /* admin: 1 page for IDENTIFY */
#define kAdminDMABytes      0x3000u
#define kIOPRPOffset        0x2000u   /* I/O: PRP-list page */
#define kIOBounceOffset     0x3000u   /* I/O: bounce buffer */
#define kIOBounceBytes      0x20000u  /* 128 KiB max transfer per command */
#define kIOQDMABytes        (kIOBounceOffset + kIOBounceBytes)

#define NVME_QUEUE_ENTRIES  64u
#define NVME_TIMEOUT_MS     5000u

void NVMe_Log(const char *fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    kprintf("[RavynNVMe] %s\n", buf);
}

IOService *
RavynNVMe::probe(IOService *provider, SInt32 *score)
{
    IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
    kprintf("RavynNVMe::probe(%p) called, pci = %p\n", provider, pci);
    if (!pci)
        return NULL;
    if (score)
        *score += 1000;
    return super::probe(provider, score);
}

bool
RavynNVMe::start(IOService *provider)
{
    kprintf("RavynNVMe::start(%p) called\n", provider);
    fProvider = OSDynamicCast(IOPCIDevice, provider);
    if (!fProvider || !super::start(provider))
        return false;

    bzero(fIOQ, sizeof(fIOQ));
    bzero(fIES, sizeof(fIES));
    fNumIOQueues = 0;
    fNumIES = 0;

    fProvider->retain();
    fProvider->setMemoryEnable(true);
    fProvider->setBusMasterEnable(true);

    uint16_t vendor    = fProvider->configRead16(kIOPCIConfigVendorID);
    uint16_t device    = fProvider->configRead16(kIOPCIConfigDeviceID);
    uint32_t classCode = fProvider->configRead32(kIOPCIConfigRevisionID) >> 8;
    NVMe_Log("start provider=%p pci%x,%x pciclass,%06x",
             provider, vendor, device, classCode);

    if (!mapRegisters())
        return false;

    uint64_t cap = reg64(NVME_REG_CAP);
    fDoorbellStride = 4u << NVME_CAP_DSTRD(cap);
    fQEntries = NVME_QUEUE_ENTRIES;
    if (NVME_CAP_MQES(cap) < fQEntries)
        fQEntries = NVME_CAP_MQES(cap);
    NVMe_Log("CAP=0x%llx version=0x%x dstride=%u mqes=%u",
             cap, reg32(NVME_REG_VS), fDoorbellStride, NVME_CAP_MQES(cap));

    if (!disableController()) {
        NVMe_Log("Controller did not become not-ready");
        return false;
    }

    /* Admin queue (polled). */
    if (!allocQueue(fAdmin, 0, fQEntries, false) || !setupAdminQueue()) {
        NVMe_Log("Admin queue setup failed");
        return false;
    }

    if (!enableController()) {
        NVMe_Log("Controller enable failed");
        return false;
    }

    if (!identifyController())
        NVMe_Log("IDENTIFY controller failed (continuing)");

    if (!createIOQueues()) {
        NVMe_Log("Failed to create I/O queues");
        return false;
    }

    if (!identifyNamespace(1) || fNSBlocks == 0) {
        NVMe_Log("No usable namespace 1");
        return true;
    }

    NVMe_Log("namespace 1: %llu blocks of %u bytes (%llu MiB) model='%s'",
             fNSBlocks, fLBABytes, (fNSBlocks * fLBABytes) >> 20, fModel);

    RavynNVMeDisk *diskNub = new RavynNVMeDisk();
    if (!diskNub) {
        NVMe_Log("Failed to allocate disk nub");
        return true;
    }
    if (!diskNub->initWithController(this, 1) ||
        !diskNub->attach(this) ||
        !diskNub->start(this)) {
        NVMe_Log("Disk nub bring-up failed");
        diskNub->detach(this);
        diskNub->release();
        return true;
    }
    fDiskNub = diskNub;
    fDiskNub->registerService();
    return true;
}

void
RavynNVMe::stop(IOService *provider)
{
    kprintf("RavynNVMe::stop(%p) called\n", provider);
    if (fDiskNub) {
        fDiskNub->stop(this);
        fDiskNub->detach(this);
        fDiskNub->release();
        fDiskNub = NULL;
    }
    teardownInterrupts();
    disableController();
    for (uint32_t i = 0; i < kRavynNVMeMaxIOQueues; i++)
        freeQueue(fIOQ[i]);
    freeQueue(fAdmin);
    if (fRegMap) {
        fRegMap->release();
        fRegMap = NULL;
        fRegs = NULL;
    }
    if (fRegDesc) {
        fRegDesc->release();
        fRegDesc = NULL;
    }
    if (fProvider) {
        fProvider->release();
        fProvider = NULL;
    }
    super::stop(provider);
}

void
RavynNVMe::free()
{
    fProvider = NULL;
    fRegDesc  = NULL;
    fRegMap   = NULL;
    fRegs     = NULL;
    fWorkLoop = NULL;
    fDiskNub  = NULL;
    super::free();
}

/* ---- BAR0 mapping ----------------------------------------------------- */

bool
RavynNVMe::mapRegisters(void)
{
    fRegMap = fProvider->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
    if (!fRegMap) {
        IODeviceMemory *dm = fProvider->getDeviceMemoryWithIndex(0);
        if (dm)
            fRegMap = dm->map();
    }
    if (!fRegMap) {
        /* Direct physical map of the (usually 64-bit) BAR0. */
        const uint32_t bar0 = fProvider->configRead32(kIOPCIConfigBaseAddress0);
        const uint32_t bar1 = fProvider->configRead32(kIOPCIConfigBaseAddress1);
        uint64_t phys = 0;
        if (!(bar0 & 0x1)) {
            if ((bar0 & 0x6) == 0x4)
                phys = ((uint64_t)bar1 << 32) | (uint64_t)(bar0 & ~0xFu);
            else
                phys = (uint64_t)(bar0 & ~0xFu);
        }
        if (phys) {
            fRegDesc = IOMemoryDescriptor::withPhysicalAddress(
                (IOPhysicalAddress)phys, 0x2000,
                kIODirectionNone | kIOMemoryMapperNone);
            if (fRegDesc) {
                fRegMap = fRegDesc->map(kIOMapAnywhere);
                if (fRegMap)
                    NVMe_Log("Mapped BAR0 via physical fallback (0x%llx)", phys);
            }
        }
    }
    if (!fRegMap) {
        NVMe_Log("Failed to map BAR0 registers");
        return false;
    }
    fRegs = (volatile uint8_t *)fRegMap->getVirtualAddress();
    return true;
}

/* ---- Controller enable/disable --------------------------------------- */

bool
RavynNVMe::waitReady(bool ready, uint32_t timeoutMs)
{
    for (uint32_t i = 0; i < timeoutMs; i++) {
        uint32_t csts = reg32(NVME_REG_CSTS);
        if (csts & NVME_CSTS_CFS) {
            NVMe_Log("Controller fatal status (CSTS=0x%x)", csts);
            return false;
        }
        if (((csts & NVME_CSTS_RDY) != 0) == ready)
            return true;
        IODelay(1000);
    }
    return false;
}

bool
RavynNVMe::disableController(void)
{
    uint32_t cc = reg32(NVME_REG_CC);
    if (cc & NVME_CC_EN)
        reg32w(NVME_REG_CC, cc & ~NVME_CC_EN);
    return waitReady(false, NVME_TIMEOUT_MS);
}

bool
RavynNVMe::enableController(void)
{
    uint32_t cc = NVME_CC_IOSQES(6) | NVME_CC_IOCQES(4) | NVME_CC_MPS(0) |
                  NVME_CC_AMS_RR   | NVME_CC_CSS_NVM   | NVME_CC_EN;
    reg32w(NVME_REG_CC, cc);
    return waitReady(true, NVME_TIMEOUT_MS);
}

/* ---- Queue allocation ------------------------------------------------- */

bool
RavynNVMe::allocQueue(NVMeQueue &q, uint32_t qid, uint32_t depth, bool wantIO)
{
    bzero(&q, sizeof(q));
    q.qid    = qid;
    q.depth  = depth;
    q.phase  = 1;
    q.nextCID = 1;
    q.lock = IOLockAlloc();
    if (!q.lock)
        return false;

    uint32_t bytes = wantIO ? kIOQDMABytes : kAdminDMABytes;
    q.dma = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut,
        bytes, 0x00000000FFFFF000ULL);
    if (!q.dma)
        return false;
    q.dma->prepare();
    q.virt = (volatile uint8_t *)q.dma->getBytesNoCopy();
    q.phys = q.dma->getPhysicalAddress();
    bzero((void *)q.virt, bytes);

    q.sq = (volatile NVMeCommand    *)(q.virt + kSQOffset);
    q.cq = (volatile NVMeCompletion *)(q.virt + kCQOffset);
    if (wantIO) {
        q.prpList     = (volatile uint64_t *)(q.virt + kIOPRPOffset);
        q.bounce      = (volatile uint8_t  *)(q.virt + kIOBounceOffset);
        q.prpListPhys = q.phys + kIOPRPOffset;
        q.bouncePhys  = q.phys + kIOBounceOffset;
    } else {
        q.bounce      = (volatile uint8_t  *)(q.virt + kAdminBounceOffset);
        q.bouncePhys  = q.phys + kAdminBounceOffset;
    }
    return true;
}

void
RavynNVMe::freeQueue(NVMeQueue &q)
{
    if (q.dma) {
        q.dma->complete();
        q.dma->release();
        q.dma = NULL;
    }
    if (q.lock) {
        IOLockFree(q.lock);
        q.lock = NULL;
    }
    q.virt = NULL;
    q.sq = NULL;
    q.cq = NULL;
}

bool
RavynNVMe::setupAdminQueue(void)
{
    uint32_t aqa = ((fAdmin.depth - 1) << 16) | (fAdmin.depth - 1);
    reg32w(NVME_REG_AQA, aqa);
    reg64w(NVME_REG_ASQ, fAdmin.phys + kSQOffset);
    reg64w(NVME_REG_ACQ, fAdmin.phys + kCQOffset);
    return true;
}

/* ---- I/O queue creation (with MSI-X vectors) -------------------------- */

bool
RavynNVMe::createIOQueues(void)
{
    bool irq = setupInterrupts();    /* sets fNumIOQueues, fMSIMode, fIES */

    /* Request the number of I/O queue pairs we intend to create. */
    NVMeCommand cmd;
    bzero(&cmd, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_SET_FEATURES;
    cmd.cdw10  = NVME_FEAT_NUM_QUEUES;
    cmd.cdw11  = ((fNumIOQueues - 1) << 16) | (fNumIOQueues - 1);
    acquireQueue(fAdmin);
    bool ok = submitCmd(fAdmin, &cmd, NULL);
    releaseQueue(fAdmin);
    if (!ok)
        NVMe_Log("Set Features (num queues) failed (continuing)");

    for (uint32_t i = 0; i < fNumIOQueues; i++) {
        if (!allocQueue(fIOQ[i], i + 1, fQEntries, true))
            return false;
        fIOQ[i].vector          = fMSIMode ? i : 0;
        fIOQ[i].interruptDriven = irq;
        if (!createIOQueue(fIOQ[i]))
            return false;
    }
    NVMe_Log("created %u I/O queue(s), completion mode = %s",
             fNumIOQueues, irq ? (fMSIMode ? "MSI-X" : "legacy IRQ") : "polled");
    return true;
}

bool
RavynNVMe::createIOQueue(NVMeQueue &q)
{
    NVMeCommand cmd;
    bool ok;

    /* Create I/O Completion Queue. cdw11 bit0 = contiguous, bit1 = IRQ enable,
     * bits 16..31 = MSI-X vector. */
    bzero(&cmd, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_CREATE_CQ;
    cmd.prp1   = q.phys + kCQOffset;
    cmd.cdw10  = ((q.depth - 1) << 16) | q.qid;
    cmd.cdw11  = q.interruptDriven ? ((q.vector << 16) | 0x3) : 0x1;
    acquireQueue(fAdmin);
    ok = submitCmd(fAdmin, &cmd, NULL);
    releaseQueue(fAdmin);
    if (!ok) {
        NVMe_Log("Create I/O CQ %u failed", q.qid);
        return false;
    }

    /* Create I/O Submission Queue bound to the CQ of the same id. */
    bzero(&cmd, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_CREATE_SQ;
    cmd.prp1   = q.phys + kSQOffset;
    cmd.cdw10  = ((q.depth - 1) << 16) | q.qid;
    cmd.cdw11  = (q.qid << 16) | 0x1;
    acquireQueue(fAdmin);
    ok = submitCmd(fAdmin, &cmd, NULL);
    releaseQueue(fAdmin);
    if (!ok) {
        NVMe_Log("Create I/O SQ %u failed", q.qid);
        return false;
    }
    return true;
}

/* ---- Interrupts ------------------------------------------------------- */

bool
RavynNVMe::setupInterrupts(void)
{
    fWorkLoop = IOWorkLoop::workLoop();
    if (!fWorkLoop) {
        NVMe_Log("No workloop; falling back to polling");
        fNumIOQueues = 1;
        return false;
    }

    int msgCount = 0;
    bool haveLevel = false;
    for (int idx = 0; idx < 64; idx++) {
        int type = 0;
        if (fProvider->getInterruptType(idx, &type) != kIOReturnSuccess)
            break;
        if (type & kIOInterruptTypePCIMessaged)
            msgCount++;
        else if (idx == 0)
            haveLevel = true;
    }
    NVMe_Log("interrupts available: messaged=%d level=%d", msgCount, haveLevel);

    IOInterruptEventSource::Action action = OSMemberFunctionCast(
        IOInterruptEventSource::Action, this, &RavynNVMe::interruptAction);

    if (msgCount > 0) {
        fMSIMode = true;
        fNumIOQueues = (uint32_t)msgCount;
        if (fNumIOQueues > kRavynNVMeMaxIOQueues)
            fNumIOQueues = kRavynNVMeMaxIOQueues;
        for (uint32_t v = 0; v < fNumIOQueues; v++) {
            IOInterruptEventSource *ies =
                IOInterruptEventSource::interruptEventSource(this, action, fProvider, v);
            if (!ies || fWorkLoop->addEventSource(ies) != kIOReturnSuccess) {
                if (ies) ies->release();
                NVMe_Log("MSI-X vector %u registration failed", v);
                break;
            }
            ies->enable();
            fIES[fNumIES++] = ies;
        }
        if (fNumIES > 0) {
            fNumIOQueues = fNumIES;       /* one I/O queue per registered vector */
            return true;
        }
        fMSIMode = false;                 /* fall through to legacy/polled */
    }

    if (haveLevel) {
        IOInterruptEventSource *ies =
            IOInterruptEventSource::interruptEventSource(this, action, fProvider, 0);
        if (ies && fWorkLoop->addEventSource(ies) == kIOReturnSuccess) {
            ies->enable();
            fIES[0] = ies;
            fNumIES = 1;
            fMSIMode = false;
            fNumIOQueues = kRavynNVMeMaxIOQueues;   /* all share the one line */
            return true;
        }
        if (ies) ies->release();
    }

    NVMe_Log("No usable interrupts; using polled completion");
    fNumIOQueues = 1;
    return false;
}

void
RavynNVMe::teardownInterrupts(void)
{
    for (uint32_t i = 0; i < fNumIES; i++) {
        if (!fIES[i])
            continue;
        fIES[i]->disable();
        if (fWorkLoop)
            fWorkLoop->removeEventSource(fIES[i]);
        fIES[i]->release();
        fIES[i] = NULL;
    }
    fNumIES = 0;
    if (fWorkLoop) {
        fWorkLoop->release();
        fWorkLoop = NULL;
    }
}

/* Runs on the workloop thread when any I/O completion vector fires.  We don't
 * rely on which vector it was: sweep every I/O completion queue and wake any
 * thread waiting on one that posted a completion. */
void
RavynNVMe::interruptAction(IOInterruptEventSource *src, int count)
{
    for (uint32_t i = 0; i < fNumIOQueues; i++) {
        NVMeQueue &q = fIOQ[i];
        if (!q.lock)
            continue;
        IOLockLock(q.lock);
        processCompletionsLocked(q);
        if (q.done)
            IOLockWakeup(q.lock, (void *)&q.done, true);
        IOLockUnlock(q.lock);
    }
}

/* Drain a CQ; caller holds q.lock.  Depth-1-per-queue (the 'busy' token) means
 * at most one outstanding command, so at most one completion per submit. */
void
RavynNVMe::processCompletionsLocked(NVMeQueue &q)
{
    volatile NVMeCompletion *cqe = &q.cq[q.cqHead];
    while (NVME_CQE_PHASE(cqe->status) == q.phase) {
        q.cstatus = NVME_CQE_STATUS(cqe->status);
        q.cqHead  = (q.cqHead + 1) % q.depth;
        if (q.cqHead == 0)
            q.phase ^= 1;
        reg32w(doorbellOffset(q.qid, true), q.cqHead);
        q.done = true;
        cqe = &q.cq[q.cqHead];
    }
}

/* ---- Queue ownership + command submission ----------------------------- */

void
RavynNVMe::acquireQueue(NVMeQueue &q)
{
    IOLockLock(q.lock);
    while (q.busy)
        IOLockSleep(q.lock, (void *)&q.busy, THREAD_UNINT);
    q.busy = true;
    IOLockUnlock(q.lock);
}

void
RavynNVMe::releaseQueue(NVMeQueue &q)
{
    IOLockLock(q.lock);
    q.busy = false;
    IOLockWakeup(q.lock, (void *)&q.busy, true);
    IOLockUnlock(q.lock);
}

bool
RavynNVMe::submitCmd(NVMeQueue &q, NVMeCommand *cmd, NVMeCompletion *cqeOut)
{
    (void)cqeOut;
    IOLockLock(q.lock);

    cmd->commandId = q.nextCID++;
    memcpy((void *)&q.sq[q.sqTail], cmd, sizeof(NVMeCommand));
    __asm__ volatile("sfence" ::: "memory");
    q.sqTail = (q.sqTail + 1) % q.depth;
    q.done    = false;
    q.cstatus = 0xFFFF;
    reg32w(doorbellOffset(q.qid, false), q.sqTail);

    if (q.interruptDriven) {
        AbsoluteTime deadline;
        clock_interval_to_deadline(NVME_TIMEOUT_MS, 1000000ULL, &deadline);
        while (!q.done) {
            int w = IOLockSleepDeadline(q.lock, (void *)&q.done,
                                        deadline, THREAD_UNINT);
            if (w == THREAD_TIMED_OUT)
                break;
        }
        if (!q.done)             /* missed-interrupt safety sweep */
            processCompletionsLocked(q);
    } else {
        for (uint32_t i = 0; i < NVME_TIMEOUT_MS * 100u && !q.done; i++) {
            processCompletionsLocked(q);
            if (q.done)
                break;
            IODelay(10);
        }
    }

    bool ok = q.done && (q.cstatus == 0);
    if (!q.done)
        NVMe_Log("queue %u cmd 0x%x timed out", q.qid, cmd->opcode);
    else if (q.cstatus != 0)
        NVMe_Log("queue %u cmd 0x%x status 0x%x", q.qid, cmd->opcode, q.cstatus);
    IOLockUnlock(q.lock);
    return ok;
}

void
RavynNVMe::buildPRP(NVMeQueue &q, uint32_t byteCount, uint64_t &prp1, uint64_t &prp2)
{
    prp1 = q.bouncePhys;
    if (byteCount <= NVME_PAGE_SIZE) {
        prp2 = 0;
    } else if (byteCount <= 2u * NVME_PAGE_SIZE) {
        prp2 = q.bouncePhys + NVME_PAGE_SIZE;
    } else {
        uint32_t nPages = (byteCount + NVME_PAGE_SIZE - 1) / NVME_PAGE_SIZE;
        for (uint32_t i = 1; i < nPages; i++)
            q.prpList[i - 1] = q.bouncePhys + (uint64_t)i * NVME_PAGE_SIZE;
        prp2 = q.prpListPhys;
    }
}

NVMeQueue *
RavynNVMe::pickIOQueue(void)
{
    if (fNumIOQueues == 0)
        return NULL;
    uint32_t i = ((uint32_t)OSIncrementAtomic(&fQueueRR)) % fNumIOQueues;
    return &fIOQ[i];
}

/* ---- IDENTIFY (admin, polled) ---------------------------------------- */

bool
RavynNVMe::identifyController(void)
{
    NVMeCommand cmd;
    uint64_t prp1, prp2;

    acquireQueue(fAdmin);
    bzero((void *)fAdmin.bounce, NVME_PAGE_SIZE);
    buildPRP(fAdmin, NVME_PAGE_SIZE, prp1, prp2);
    bzero(&cmd, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_IDENTIFY;
    cmd.prp1   = prp1;
    cmd.prp2   = prp2;
    cmd.cdw10  = NVME_IDENTIFY_CNS_CTRL;
    bool ok = submitCmd(fAdmin, &cmd, NULL);
    if (ok) {
        const NVMeIdentifyController *id =
            (const NVMeIdentifyController *)fAdmin.bounce;
        nvmeTrimString(fModel,    sizeof(fModel),    id->mn, sizeof(id->mn));
        nvmeTrimString(fSerial,   sizeof(fSerial),   id->sn, sizeof(id->sn));
        nvmeTrimString(fFirmware, sizeof(fFirmware), id->fr, sizeof(id->fr));
        NVMe_Log("controller model='%s' serial='%s' fw='%s'",
                 fModel, fSerial, fFirmware);
    }
    releaseQueue(fAdmin);
    return ok;
}

bool
RavynNVMe::identifyNamespace(uint32_t nsid)
{
    NVMeCommand cmd;
    uint64_t prp1, prp2;

    acquireQueue(fAdmin);
    bzero((void *)fAdmin.bounce, NVME_PAGE_SIZE);
    buildPRP(fAdmin, NVME_PAGE_SIZE, prp1, prp2);
    bzero(&cmd, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_IDENTIFY;
    cmd.nsid   = nsid;
    cmd.prp1   = prp1;
    cmd.prp2   = prp2;
    cmd.cdw10  = NVME_IDENTIFY_CNS_NS;
    bool ok = submitCmd(fAdmin, &cmd, NULL);
    if (ok) {
        const NVMeIdentifyNamespace *ns =
            (const NVMeIdentifyNamespace *)fAdmin.bounce;
        uint32_t fmtIndex = ns->flbas & 0x0F;
        uint32_t lbads    = NVME_LBAF_LBADS(ns->lbaf[fmtIndex]);
        fNSBlocks = ns->nsze;
        fLBABytes = (lbads >= 9 && lbads <= 20) ? (1u << lbads) : 512u;
    }
    releaseQueue(fAdmin);
    return ok;
}

/* ---- Block I/O (called by RavynNVMeDisk) ----------------------------- */

IOReturn
RavynNVMe::doRead(uint32_t nsid, uint64_t lba, uint32_t blocks,
                  IOMemoryDescriptor *buffer, uint64_t bufOff)
{
    if (!fLBABytes || !buffer)
        return kIOReturnNoDevice;
    NVMeQueue *q = pickIOQueue();
    if (!q)
        return kIOReturnNoDevice;

    const uint32_t maxBlocks = kIOBounceBytes / fLBABytes;
    uint32_t remaining = blocks;
    uint64_t curLBA    = lba;
    uint64_t curOff    = bufOff;
    IOReturn ret       = kIOReturnSuccess;

    acquireQueue(*q);
    while (remaining > 0) {
        uint32_t chunk = (remaining > maxBlocks) ? maxBlocks : remaining;
        uint32_t bytes = chunk * fLBABytes;
        uint64_t prp1, prp2;
        NVMeCommand cmd;

        buildPRP(*q, bytes, prp1, prp2);
        bzero(&cmd, sizeof(cmd));
        cmd.opcode = NVME_IO_READ;
        cmd.nsid   = nsid;
        cmd.prp1   = prp1;
        cmd.prp2   = prp2;
        cmd.cdw10  = (uint32_t)(curLBA & 0xFFFFFFFF);
        cmd.cdw11  = (uint32_t)(curLBA >> 32);
        cmd.cdw12  = chunk - 1;

        if (!submitCmd(*q, &cmd, NULL)) {
            ret = kIOReturnIOError;
            break;
        }
        if (buffer->writeBytes(curOff, (void *)q->bounce, bytes) != bytes) {
            ret = kIOReturnUnderrun;
            break;
        }
        curLBA    += chunk;
        curOff    += bytes;
        remaining -= chunk;
    }
    releaseQueue(*q);
    return ret;
}

IOReturn
RavynNVMe::doWrite(uint32_t nsid, uint64_t lba, uint32_t blocks,
                   IOMemoryDescriptor *buffer, uint64_t bufOff)
{
    if (!fLBABytes || !buffer)
        return kIOReturnNoDevice;
    NVMeQueue *q = pickIOQueue();
    if (!q)
        return kIOReturnNoDevice;

    const uint32_t maxBlocks = kIOBounceBytes / fLBABytes;
    uint32_t remaining = blocks;
    uint64_t curLBA    = lba;
    uint64_t curOff    = bufOff;
    IOReturn ret       = kIOReturnSuccess;

    acquireQueue(*q);
    while (remaining > 0) {
        uint32_t chunk = (remaining > maxBlocks) ? maxBlocks : remaining;
        uint32_t bytes = chunk * fLBABytes;
        uint64_t prp1, prp2;
        NVMeCommand cmd;

        if (buffer->readBytes(curOff, (void *)q->bounce, bytes) != bytes) {
            ret = kIOReturnUnderrun;
            break;
        }
        buildPRP(*q, bytes, prp1, prp2);
        bzero(&cmd, sizeof(cmd));
        cmd.opcode = NVME_IO_WRITE;
        cmd.nsid   = nsid;
        cmd.prp1   = prp1;
        cmd.prp2   = prp2;
        cmd.cdw10  = (uint32_t)(curLBA & 0xFFFFFFFF);
        cmd.cdw11  = (uint32_t)(curLBA >> 32);
        cmd.cdw12  = chunk - 1;

        if (!submitCmd(*q, &cmd, NULL)) {
            ret = kIOReturnIOError;
            break;
        }
        curLBA    += chunk;
        curOff    += bytes;
        remaining -= chunk;
    }
    releaseQueue(*q);
    return ret;
}

IOReturn
RavynNVMe::doFlush(uint32_t nsid)
{
    NVMeQueue *q = pickIOQueue();
    if (!q)
        return kIOReturnNoDevice;

    NVMeCommand cmd;
    bzero(&cmd, sizeof(cmd));
    cmd.opcode = NVME_IO_FLUSH;
    cmd.nsid   = nsid;

    acquireQueue(*q);
    bool ok = submitCmd(*q, &cmd, NULL);
    releaseQueue(*q);
    return ok ? kIOReturnSuccess : kIOReturnIOError;
}

/* ---- Helpers ---------------------------------------------------------- */

void
RavynNVMe::nvmeTrimString(char *dst, size_t dstLen, const char *src, size_t srcLen)
{
    size_t n = (srcLen < dstLen - 1) ? srcLen : dstLen - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
    while (n > 0 && (dst[n - 1] == ' ' || dst[n - 1] == '\0'))
        dst[--n] = '\0';
}
