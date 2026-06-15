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

#ifndef _RAVYN_NVME_H
#define _RAVYN_NVME_H

#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/pci/IOPCIDevice.h>
#include "NVMe.h"

extern void NVMe_Log(const char *fmt, ...);

class RavynNVMeDisk;   /* forward declaration */

/* Up to this many I/O queue pairs (one MSI-X vector each). */
#define kRavynNVMeMaxIOQueues   4u

/*
 * One NVMe queue pair (submission + completion).  Each pair has its own
 * physically-contiguous DMA buffer holding the SQ, CQ, a PRP-list page and a
 * bounce buffer, plus a lock used both as a short-term mutex and for the
 * interrupt-driven sleep/wake handshake.  'busy' is the ownership token held by
 * a thread for the duration of a (possibly multi-chunk) transfer so the bounce
 * buffer is never shared concurrently.
 */
struct NVMeQueue {
    uint32_t                   qid;          /* 0 = admin, 1..N = I/O */
    uint32_t                   vector;       /* MSI-X vector for this CQ */
    uint32_t                   depth;        /* entries per queue */
    bool                       interruptDriven;

    IOBufferMemoryDescriptor * dma;
    volatile uint8_t         * virt;
    uint64_t                   phys;

    volatile NVMeCommand     * sq;
    volatile NVMeCompletion  * cq;
    volatile uint64_t        * prpList;
    volatile uint8_t         * bounce;
    uint64_t                   prpListPhys;
    uint64_t                   bouncePhys;

    uint32_t                   sqTail;
    uint32_t                   cqHead;
    uint8_t                    phase;        /* expected CQE phase tag */
    uint16_t                   nextCID;

    IOLock                   * lock;
    bool                       busy;         /* queue owned by a transfer */
    volatile bool              done;         /* a completion was posted */
    uint16_t                   cstatus;      /* last completion status field */

    IOInterruptEventSource   * ies;          /* per-vector source (may be NULL) */
};

/*
 * A polled/interrupt-driven NVMe controller driver.  Creates an admin queue
 * pair plus up to kRavynNVMeMaxIOQueues I/O queue pairs, one MSI-X vector per
 * I/O queue when message interrupts are available (falling back to a single
 * legacy line, then to polling).  Only namespace 1 is published.
 */
class RavynNVMe : public IOService
{
    OSDeclareDefaultStructors(RavynNVMe);
    friend class RavynNVMeDisk;

public:
    IOService *probe(IOService *provider, SInt32 *score) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;

    /* Public block I/O interface (called by RavynNVMeDisk) */
    IOReturn doRead (uint32_t nsid, uint64_t lba, uint32_t blocks,
                     IOMemoryDescriptor *buffer, uint64_t bufOff);
    IOReturn doWrite(uint32_t nsid, uint64_t lba, uint32_t blocks,
                     IOMemoryDescriptor *buffer, uint64_t bufOff);
    IOReturn doFlush(uint32_t nsid);

    /* Geometry / identity reported by IDENTIFY */
    uint64_t    blockCount(void) const  { return fNSBlocks; }
    uint32_t    blockSize(void)  const  { return fLBABytes; }
    const char *modelString(void)    const { return fModel; }
    const char *serialString(void)   const { return fSerial; }
    const char *firmwareString(void) const { return fFirmware; }

private:
    inline uint32_t reg32(uint32_t off) const
        { return *(volatile uint32_t *)(fRegs + off); }
    inline void reg32w(uint32_t off, uint32_t val) const
        { *(volatile uint32_t *)(fRegs + off) = val; }
    inline uint64_t reg64(uint32_t off) const
        { return *(volatile uint64_t *)(fRegs + off); }
    inline void reg64w(uint32_t off, uint64_t val) const
        { *(volatile uint64_t *)(fRegs + off) = val; }

    inline uint32_t doorbellOffset(uint32_t queue, bool completion) const
        { return NVME_REG_DBS + ((2 * queue + (completion ? 1 : 0)) * fDoorbellStride); }

    bool mapRegisters(void);

    bool enableController(void);
    bool disableController(void);
    bool waitReady(bool ready, uint32_t timeoutMs);

    /* Queue lifecycle */
    bool allocQueue(NVMeQueue &q, uint32_t qid, uint32_t depth, bool wantIO);
    void freeQueue(NVMeQueue &q);
    bool setupAdminQueue(void);
    bool createIOQueues(void);
    bool createIOQueue(NVMeQueue &q);

    bool identifyController(void);
    bool identifyNamespace(uint32_t nsid);

    /* Interrupts */
    bool setupInterrupts(void);     /* returns true if any I/O queue is IRQ-driven */
    void teardownInterrupts(void);
    void interruptAction(IOInterruptEventSource *src, int count);
    void processCompletionsLocked(NVMeQueue &q);

    /* Command submission: acquire/release own the queue across a transfer;
     * submitCmd places one command and waits (interrupt or poll). */
    void acquireQueue(NVMeQueue &q);
    void releaseQueue(NVMeQueue &q);
    bool submitCmd(NVMeQueue &q, NVMeCommand *cmd, NVMeCompletion *cqeOut);

    void buildPRP(NVMeQueue &q, uint32_t byteCount, uint64_t &prp1, uint64_t &prp2);
    NVMeQueue *pickIOQueue(void);

    static void nvmeTrimString(char *dst, size_t dstLen,
                               const char *src, size_t srcLen);

    IOPCIDevice         * fProvider;
    IOMemoryDescriptor  * fRegDesc;     /* set only on the physical-map fallback */
    IOMemoryMap         * fRegMap;
    volatile uint8_t    * fRegs;
    RavynNVMeDisk       * fDiskNub;

    IOWorkLoop          * fWorkLoop;
    IOInterruptEventSource * fIES[kRavynNVMeMaxIOQueues];
    uint32_t              fNumIES;
    bool                  fMSIMode;

    NVMeQueue             fAdmin;
    NVMeQueue             fIOQ[kRavynNVMeMaxIOQueues];
    uint32_t              fNumIOQueues;
    volatile SInt32       fQueueRR;     /* round-robin selector */

    uint32_t fDoorbellStride;           /* in bytes */
    uint32_t fQEntries;                 /* queue depth (admin + I/O) */

    /* Namespace 1 geometry */
    uint64_t fNSBlocks;
    uint32_t fLBABytes;
    char     fModel[41];
    char     fSerial[21];
    char     fFirmware[9];
};

#endif /* _RAVYN_NVME_H */
