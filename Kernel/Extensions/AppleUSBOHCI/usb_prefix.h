/*
 * ravynOS USB port: forced prefix header.
 *
 * The vendored IOUSBFamily-560.4.2 sources include <IOKit/IOCommandGate.h>
 * before any header that defines IOInterruptHandler, so the
 * IOLib.h -> IOLocks.h -> machine/machine_routines.h chain fails to compile
 * ("unknown type name 'IOInterruptHandler'"). Pull in the typedef up front,
 * before the machine header is reached. Guarded so the C kmod_info.c compile
 * (which never reaches that chain) is unaffected.
 */
#ifdef __cplusplus
#include <IOKit/IOTypes.h>
#include <IOKit/IOReturn.h>
class IOService;
#include <IOKit/IOInterrupts.h>
#endif
