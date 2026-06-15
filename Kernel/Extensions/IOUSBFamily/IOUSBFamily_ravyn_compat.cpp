/*
 * ravynOS USB port compatibility shims.
 *
 * Definitions that the apple-oss IOUSBFamily-560.4.2 open-source drop declares
 * but does not ship the implementation for, leaving unresolved symbols at kxld
 * time. Kept separate from the vendored sources so the port deltas are obvious.
 */

#include <IOKit/usb/IOUSBControllerV3.h>

// IOUSBControllerV3.h declares GetErrataBits as a virtual override ("we override
// this one to add some stuff which requires the _device iVar"), but no .cpp in
// the public 560.4.2 sources defines IOUSBControllerV3::GetErrataBits, so the V3
// vtable slot is unresolved. The base IOUSBController::GetErrataBits provides the
// errata-table lookup; the device-specific additions Apple referred to are not in
// the open-source drop and are not needed under QEMU. Subclasses such as
// AppleUSBXHCI override this with their own hardware logic. Delegate to the base.
UInt32
IOUSBControllerV3::GetErrataBits(UInt16 vendorID, UInt16 deviceID, UInt16 revisionID)
{
    return IOUSBController::GetErrataBits(vendorID, deviceID, revisionID);
}
