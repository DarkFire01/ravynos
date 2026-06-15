/*
 * RavynNVMeDisk: minimal IOBlockStorageDevice nub for NVMe namespaces
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
#include <IOKit/storage/IOStorage.h>
#include "RavynNVMeDisk.h"

#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors(RavynNVMeDisk, IOBlockStorageDevice);


bool
RavynNVMeDisk::initWithController(RavynNVMe *parent, UInt32 nsid)
{
    char buf[32];

    if (!parent) return false;
    if (!init(NULL)) return false;
    fParent = parent;
    fNSID   = nsid;
    const char *model    = parent->modelString();
    const char *firmware = parent->firmwareString();
    strlcpy(fVendorStr,   "NVMe", sizeof(fVendorStr));
    strlcpy(fProductStr,  (model    && model[0])    ? model    : "NVMe Disk", sizeof(fProductStr));
    strlcpy(fRevisionStr, (firmware && firmware[0]) ? firmware : "0000",      sizeof(fRevisionStr));
    setProperty(kIOBlockStorageDeviceTypeKey, kIOBlockStorageDeviceTypeGeneric);
    char loc[8];
    snprintf(loc, sizeof(loc), "%u", (unsigned)nsid);
    setLocation(loc);
    snprintf(buf, sizeof(buf) - 1, "nvme%u", (unsigned)nsid);
    setName(buf);
    NVMe_Log("%s init nsid=%u model=%s fw=%s blocks=0x%llx bsize=%u",
             buf, (unsigned)nsid, fProductStr, fRevisionStr,
             parent->blockCount(), parent->blockSize());
    return true;
}

bool
RavynNVMeDisk::start(IOService *provider)
{
    return super::start(provider);
}

void
RavynNVMeDisk::stop(IOService *provider)
{
    super::stop(provider);
}

void
RavynNVMeDisk::free()
{
    fParent = NULL;
    super::free();
}


IOReturn
RavynNVMeDisk::doAsyncReadWrite(IOMemoryDescriptor  * buffer,
                                UInt64                block,
                                UInt64                nblks,
                                IOStorageAttributes * attributes,
                                IOStorageCompletion * completion)
{
    if (!fParent || nblks == 0 || !buffer) {
        IOStorage::complete(completion, kIOReturnBadArgument, 0);
        return kIOReturnBadArgument;
    }

    UInt64 maxBlock = fParent->blockCount();
    if (maxBlock == 0 || block >= maxBlock || nblks > (maxBlock - block)) {
        NVMe_Log("nvme%u rejecting I/O block=%llu nblks=%llu max=%llu",
                 (unsigned)fNSID, block, nblks, maxBlock);
        IOStorage::complete(completion, kIOReturnBadArgument, 0);
        return kIOReturnBadArgument;
    }

    IOReturn ioret = buffer->prepare();
    if (ioret != kIOReturnSuccess) {
        IOStorage::complete(completion, ioret, 0);
        return ioret;
    }

    const bool isWrite  = ((buffer->getDirection() & kIODirectionOut) != 0);
    UInt64     blkBytes = fParent->blockSize();
    UInt64     totBytes = nblks * blkBytes;
    IOReturn   ret;

    if (block < 64) {
        NVMe_Log("nvme%u %s block=%llu nblks=%llu bytes=%llu",
                 (unsigned)fNSID, isWrite ? "write" : "read",
                 block, nblks, totBytes);
    }

    if (isWrite)
        ret = fParent->doWrite(fNSID, block, (UInt32)nblks, buffer, 0);
    else
        ret = fParent->doRead (fNSID, block, (UInt32)nblks, buffer, 0);
    buffer->complete();
    IOStorage::complete(completion, ret, (ret == kIOReturnSuccess) ? totBytes : 0);

    return ret;
}

IOReturn
RavynNVMeDisk::doSynchronize(UInt64 block,
                             UInt64 nblks,
                             IOStorageSynchronizeOptions options)
{
    if (!fParent) return kIOReturnNoDevice;
    return fParent->doFlush(fNSID);
}

IOReturn
RavynNVMeDisk::doEjectMedia(void)
{
    return kIOReturnUnsupported;
}

IOReturn
RavynNVMeDisk::doFormatMedia(UInt64 byteCapacity)
{
    return kIOReturnUnsupported;
}

UInt32
RavynNVMeDisk::doGetFormatCapacities(UInt64 *capacities,
                                     UInt32  capacitiesMaxCount) const
{
    if (capacities && capacitiesMaxCount >= 1 && fParent)
        capacities[0] = fParent->blockCount() * fParent->blockSize();
    return 1;
}

char *RavynNVMeDisk::getVendorString(void)              { return fVendorStr; }
char *RavynNVMeDisk::getProductString(void)             { return fProductStr; }
char *RavynNVMeDisk::getRevisionString(void)            { return fRevisionStr; }
char *RavynNVMeDisk::getAdditionalDeviceInfoString(void){ return (char *)""; }


IOReturn
RavynNVMeDisk::reportBlockSize(UInt64 *blockSize)
{
    *blockSize = fParent ? fParent->blockSize() : 512;
    return kIOReturnSuccess;
}

IOReturn
RavynNVMeDisk::reportEjectability(bool *isEjectable)
{
    *isEjectable = false;
    return kIOReturnSuccess;
}

IOReturn
RavynNVMeDisk::reportMaxValidBlock(UInt64 *maxBlock)
{
    UInt64 blocks = fParent ? fParent->blockCount() : 0;
    *maxBlock = blocks > 0 ? blocks - 1 : 0;
    return kIOReturnSuccess;
}

IOReturn
RavynNVMeDisk::reportMediaState(bool *mediaPresent, bool *changedState)
{
    if (mediaPresent) *mediaPresent = (fParent != NULL);
    if (changedState) *changedState = false;
    return kIOReturnSuccess;
}

IOReturn
RavynNVMeDisk::reportRemovability(bool *isRemovable)
{
    *isRemovable = false;
    return kIOReturnSuccess;
}

IOReturn
RavynNVMeDisk::reportWriteProtection(bool *isWriteProtected)
{
    *isWriteProtected = false;
    return kIOReturnSuccess;
}
