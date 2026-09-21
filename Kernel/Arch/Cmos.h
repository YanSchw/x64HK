#pragma once
#include "Types.h"

// CMOS RAM plus the real time clock, reached through an index/data port pair.
// The top bit of the index port doubles as the NMI mask, so every write has to
// preserve it.
namespace Cmos {

enum class Register : uint8_t {
    SECOND = 0x0,
    ALARM_SECOND = 0x1,
    MINUTE = 0x2,
    ALARM_MINUTE = 0x3,
    HOUR = 0x4,
    ALARM_HOUR = 0x5,
    WEEKDAY = 0x6,
    DAY_OF_MONTH = 0x7,
    MONTH = 0x8,
    YEAR = 0x9,
    STATUS_A = 0xa,
    STATUS_B = 0xb,
    STATUS_C = 0xc,
    STATUS_D = 0xd,
    STATUS_DIAGNOSE = 0xe,
    STATUS_SHUTDOWN = 0xf,
};

uint8_t Read(Register InRegister);
void Write(Register InRegister, uint8_t InValue);

namespace Nmi {
void Enable();
void Disable();
bool IsEnabled();
}  // namespace Nmi

}  // namespace Cmos
