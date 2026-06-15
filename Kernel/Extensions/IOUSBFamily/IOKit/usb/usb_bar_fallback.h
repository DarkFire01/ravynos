/*
 * ravynOS USB port: physical-BAR mapping fallback.
 *
 * IOPCIFamily on this platform doesn't always publish IODeviceMemory for a PCI
 * device's BARs (QEMU), so mapDeviceMemoryWithIndex()/WithRegister() return NULL.
 * Map the memory BAR directly from the physical address read out of config
 * space, exactly as the ravynnvme / RavynAHCIPort / AppleUSBXHCI drivers do.
 */
#ifndef _USB_BAR_FALLBACK_H
#define _USB_BAR_FALLBACK_H
#ifdef __cplusplus

#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>

// Map a memory BAR (by its config register, e.g. kIOPCIConfigBaseAddress0).
// Returns an IOMemoryMap* the caller retains, or 0. Tries the normal IOKit path
// first, then the physical fallback.
static inline IOMemoryMap *
RavynMapPCIMemoryBar(IOPCIDevice * device, uint8_t barReg, uint32_t length)
{
    IOMemoryMap * map = device->mapDeviceMemoryWithRegister(barReg);
    if (map) return (map);

    uint32_t lo   = device->configRead32(barReg);
    uint64_t phys = 0;
    if (!(lo & 0x1))                       // memory-space BAR
    {
        if ((lo & 0x6) == 0x4)             // 64-bit BAR
        {
            uint32_t hi = device->configRead32(barReg + 4);
            phys = ((uint64_t) hi << 32) | (uint64_t)(lo & ~0xFu);
        }
        else
        {
            phys = (uint64_t)(lo & ~0xFu);
        }
    }
    if (!phys) return (0);

    IOMemoryDescriptor * md = IOMemoryDescriptor::withPhysicalAddress(
            (IOPhysicalAddress) phys, length, kIODirectionInOut | kIOMemoryMapperNone);
    if (!md) return (0);
    IOMemoryMap * m = md->map(kIOMapAnywhere | kIOMapInhibitCache);
    if (!m) { md->release(); return (0); }
    return (m);
}

#endif /* __cplusplus */
#endif /* _USB_BAR_FALLBACK_H */
