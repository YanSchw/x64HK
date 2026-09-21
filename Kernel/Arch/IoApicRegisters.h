#pragma once
#include "Types.h"

namespace IoApic {

using Index = uint32_t;
using Register = uint32_t;

enum RegisterIndex : Index {
    IDENTIFICATION = 0x00,
    VERSION = 0x01,
    REDIRECTION_TABLE = 0x10,  ///< Two registers per entry
};

union Identification {
    struct {
        uint32_t : 24, Id : 4, : 4;
    };
    Register Value;
    explicit Identification(Register InValue) : Value(InValue) {}
} __attribute__((packed));
static_assert(sizeof(Identification) == 4, "IoApic::Identification has the wrong size");

enum class DeliveryMode : uint8_t {
    FIXED = 0,            ///< Deliver to every core in the destination mask
    LOWEST_PRIORITY = 1,  ///< Deliver to the least busy core in the mask
    SMI = 2,
    NMI = 4,
    INIT = 5,
    EXTERNAL = 7,
};

enum class DestinationMode : uint8_t {
    PHYSICAL = 0,
    LOGICAL = 1,
};

enum class Polarity : uint8_t {
    HIGH = 0,
    LOW = 1,
};

enum class TriggerMode : uint8_t {
    EDGE = 0,
    LEVEL = 1,  ///< Redelivered until acknowledged, so a lost IRQ is recoverable
};

enum class InterruptMask : uint8_t {
    UNMASKED = 0,
    MASKED = 1,
};

/// One redirection table entry: which vector an external interrupt raises and
/// on which core. 64 bits wide, but the registers are only 32, so it is written
/// in two halves.
union RedirectionTableEntry {
    struct {
        uint64_t Vector : 8;
        DeliveryMode Delivery : 3;
        DestinationMode Destination : 1;
        uint64_t DeliveryStatus : 1;  ///< Read only
        Polarity Polarity : 1;
        uint64_t RemoteIrr : 1;  ///< Read only, cleared by the LAPIC's EOI
        TriggerMode Trigger : 1;
        InterruptMask Mask : 1;
        uint64_t : 39;
        uint64_t DestinationId : 8;
    } __attribute__((packed));
    struct {
        Register ValueLow;
        Register ValueHigh;
    } __attribute__((packed));

    RedirectionTableEntry(Register InLow, Register InHigh) : ValueLow(InLow), ValueHigh(InHigh) {}
};
static_assert(sizeof(RedirectionTableEntry) == 8, "IoApic::RedirectionTableEntry has the wrong size");

}  // namespace IoApic
