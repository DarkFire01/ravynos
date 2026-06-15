/*
 * ravynOS AppleUSBHub port compatibility shims.
 *
 * AppleUSBHub::SetPortPower and AppleUSBHubPort::HandleLinkState are declared in
 * the IOUSBFamily-560.4.2 headers and called by the hub sources, but their
 * definitions are absent from the open-source drop (same kind of gap as
 * IOUSBControllerV3::GetErrataBits). Provide them here.
 */

#include "AppleUSBHub.h"
#include "AppleUSBHubPort.h"
#include <IOKit/usb/USBHub.h>

// Power a hub port on/off via the standard SET_FEATURE/CLEAR_FEATURE(PORT_POWER)
// hub class request (SetPortFeature/ClearPortFeature are defined in the drop).
IOReturn
AppleUSBHub::SetPortPower(UInt16 port, UInt32 on)
{
    if (on != kHubPortPowerOff)
        return SetPortFeature(kUSBHubPortPowerFeature, port);
    return ClearPortFeature(kUSBHubPortPowerFeature, port);
}

// USB3 (SuperSpeed) port link-state change handling. The caller has already
// cached the new link state; nothing further is required for basic enumeration.
IOReturn
AppleUSBHubPort::HandleLinkState(UInt16 changeFlags, UInt16 statusFlags)
{
    return kIOReturnSuccess;
}
