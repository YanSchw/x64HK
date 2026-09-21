#include "Arch/LocalApic.h"
#include "Arch/LocalApicRegisters.h"

namespace LocalApic {
namespace Ipi {

enum class DeliveryMode : uint8_t {
    FIXED = 0,
    LOWEST_PRIORITY = 1,
    SMI = 2,
    NMI = 4,
    INIT = 5,
    STARTUP = 6,
};

enum class DestinationMode : uint8_t {
    PHYSICAL = 0,  ///< Destination is a physical APIC ID
    LOGICAL = 1,   ///< Destination is a mask of logical APIC IDs
};

enum class DeliveryStatus : uint8_t {
    IDLE = 0,
    SEND_PENDING = 1,
};

enum class Level : uint8_t {
    DEASSERT = 0,
    ASSERT = 1,
};

enum class TriggerMode : uint8_t {
    EDGE = 0,
    LEVEL = 1,
};

enum class Shorthand : uint8_t {
    NONE = 0,
    SELF = 1,
    ALL_INCLUDING_SELF = 2,
    ALL_EXCLUDING_SELF = 3,
};

/// Interrupt Command Register. Split across two 32-bit registers; writing the
/// low half is what actually fires the IPI, so it has to be written last.
union InterruptCommand {
    struct {
        uint64_t Vector : 8;
        DeliveryMode Delivery : 3;
        DestinationMode Destination : 1;
        DeliveryStatus Status : 1;  ///< Read only
        uint64_t : 1;
        Level Assert : 1;
        TriggerMode Trigger : 1;
        uint64_t : 2;
        Shorthand DestinationShorthand : 2;
        uint64_t : 36;
        uint64_t DestinationId : 8;
    } __attribute__((packed));
    struct {
        Register ValueLow;
        Register ValueHigh;
    } __attribute__((packed));

    InterruptCommand(uint8_t InDestinationId, uint8_t InVector, DestinationMode InDestinationMode,
                     DeliveryMode InDeliveryMode = DeliveryMode::FIXED,
                     TriggerMode InTriggerMode = TriggerMode::EDGE, Level InLevel = Level::ASSERT) {
        WaitForIdle();
        Vector = InVector;
        Delivery = InDeliveryMode;
        Destination = InDestinationMode;
        Assert = InLevel;
        Trigger = InTriggerMode;
        DestinationShorthand = Shorthand::NONE;
        DestinationId = InDestinationId;
    }

    InterruptCommand(Shorthand InShorthand, uint8_t InVector,
                     DeliveryMode InDeliveryMode = DeliveryMode::FIXED,
                     TriggerMode InTriggerMode = TriggerMode::EDGE, Level InLevel = Level::ASSERT) {
        WaitForIdle();
        Vector = InVector;
        Delivery = InDeliveryMode;
        Destination = DestinationMode::PHYSICAL;
        Assert = InLevel;
        Trigger = InTriggerMode;
        DestinationShorthand = InShorthand;
        DestinationId = 0;
    }

    InterruptCommand() { ValueLow = LocalApic::Read(Index::INTERRUPT_COMMAND_LOW); }

    void Send() const {
        LocalApic::Write(Index::INTERRUPT_COMMAND_HIGH, ValueHigh);
        LocalApic::Write(Index::INTERRUPT_COMMAND_LOW, ValueLow);
    }

    bool IsSendPending() {
        ValueLow = LocalApic::Read(Index::INTERRUPT_COMMAND_LOW);
        return Status == DeliveryStatus::SEND_PENDING;
    }

private:
    void WaitForIdle() {
        ValueHigh = 0;
        ValueLow = 0;
        while (IsSendPending()) {
        }
        ValueHigh = LocalApic::Read(Index::INTERRUPT_COMMAND_HIGH);
    }
};
static_assert(sizeof(InterruptCommand) == 8, "LAPIC InterruptCommand has the wrong size");

bool IsDelivered() {
    InterruptCommand command;
    return !command.IsSendPending();
}

void Send(uint8_t InDestinationApicId, uint8_t InVector) {
    InterruptCommand(InDestinationApicId, InVector, DestinationMode::PHYSICAL).Send();
}

void SendGroup(uint8_t InLogicalDestination, uint8_t InVector) {
    InterruptCommand(InLogicalDestination, InVector, DestinationMode::LOGICAL).Send();
}

void SendAll(uint8_t InVector) {
    InterruptCommand(Shorthand::ALL_INCLUDING_SELF, InVector).Send();
}

void SendOthers(uint8_t InVector) {
    InterruptCommand(Shorthand::ALL_EXCLUDING_SELF, InVector).Send();
}

void SendInit(bool InAssert) {
    // The de-assert half of the sequence is the one case that needs level
    // trigger mode and a de-asserted level.
    InterruptCommand(Shorthand::ALL_EXCLUDING_SELF, 0, DeliveryMode::INIT,
                     InAssert ? TriggerMode::EDGE : TriggerMode::LEVEL,
                     InAssert ? Level::ASSERT : Level::DEASSERT)
        .Send();
}

void SendStartup(uint8_t InVector) {
    // The vector is a page number: the target starts executing at InVector<<12
    // in real mode, which is why the trampoline has to live below 1 MiB.
    InterruptCommand(Shorthand::ALL_EXCLUDING_SELF, InVector, DeliveryMode::STARTUP).Send();
}

}  // namespace Ipi
}  // namespace LocalApic
