#include "Arch/System.h"
#include "Arch/Cmos.h"
#include "Arch/Cpu.h"
#include "Arch/IoPort.h"
#include "Debug/Output.h"

namespace System {

[[noreturn]] void Reboot() {
    DBG << "Rebooting" << EndLine;

    // Clearing the shutdown status byte stops the BIOS from taking a warm boot
    // path that expects state we never set up.
    Cmos::Write(Cmos::Register::STATUS_SHUTDOWN, 0);

    // Bit 0 of System Control Port A pulses the CPU reset line.
    IoPort(0x92).OutB(0x03);

    Cpu::Die();
}

void Shutdown() {
    // QEMU and Bochs watch these ports for a shutdown request.
    IoPort(0x604).OutW(0x2000);  // QEMU >= 2.0
    IoPort(0xb004).OutW(0x2000);  // Bochs and older QEMU
    IoPort(0x4004).OutW(0x3400);  // VirtualBox
}

}  // namespace System
