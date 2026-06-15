/*
 * ravynOS AppleUSBXHCI port compatibility shims.
 *
 * The IOUSBFamily-560.4.2 AppleUSBXHCIUIM.cpp references a few symbols that the
 * open-source drop's headers don't ship (Apple's UIM was slightly ahead of the
 * published headers). All are Intel-host-specific and never exercised on QEMU's
 * Red Hat XHCI (vendor 0x1b36). Force-included after usb_prefix.h.
 *
 * Needs an include guard: rvn.kext.mk folds CFLAGS into CXXFLAGS, so the
 * -include appears twice on the command line. Without the guard the IsPortMuxed
 * template below would be defined twice (redefinition error).
 */
#ifndef _XHCI_RAVYN_COMPAT_H
#define _XHCI_RAVYN_COMPAT_H
#ifdef __cplusplus
#include <IOKit/usb/IOUSBController.h>   /* full ErrataListEntry (sizeof) */
#include <IOKit/usb/IOUSBPriv.h>         /* kIOUSBMessageMuxFrom{XHCI<->EHCI} */

/* Intel Panther Point "SW-assist XHCI idle" errata bit. Only OR'd in for Intel
 * 8086:1e31; never set under QEMU. Use a free high bit so it can't collide with
 * the kXHCIBitN errata flags (which are low bits). */
#ifndef kErrataSWAssistXHCIIdle
#define kErrataSWAssistXHCIIdle 0x40000000
#endif

/* IsPortMuxed(): Intel XHCI<->EHCI ACPI port-mux probe, absent from this drop.
 * QEMU's XHCI exposes no muxed companion ports, so report none. Templated to
 * match the call's argument types without depending on the missing declaration. */
template <typename A, typename B, typename C, typename D>
static inline bool IsPortMuxed(A, B, C, D) { return false; }
#endif

#endif /* _XHCI_RAVYN_COMPAT_H */
