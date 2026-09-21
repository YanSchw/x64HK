#include "Debug/DebugExit.h"
#include "Arch/IoPort.h"

namespace Debug {

#ifdef TEST
/// Where Tools/RunTests.sh tells QEMU to put the device.
static const IoPort s_ExitPort(0xf4);
#endif

void RequestExit(ExitCode InCode) {
#ifdef TEST
    s_ExitPort.OutL(ToUnderlying(InCode));
#else
    (void)InCode;
#endif
}

}  // namespace Debug
