#include "Arch/LocalApic.h"
#include "Arch/LocalApicRegisters.h"
#include "Arch/CpuInterrupt.h"

namespace LocalApic {

// Architectural reset value. Overwritten from the MADT during Apic::Initialize.
volatile uintptr_t s_BaseAddress = 0xfee0'0000;

// The register unions below read on construction and write back on destruction,
// so a scoped block reads as a read-modify-write of one hardware register.

union IdentificationRegister {
    struct {
        uint32_t : 24, ApicId : 8;
    };
    Register Value;
    IdentificationRegister() : Value(Read(Index::IDENTIFICATION)) {}
} __attribute__((packed));

union VersionRegister {
    struct {
        uint32_t Version : 8, : 8, MaxLvtEntry : 8, SuppressEoiBroadcast : 1, : 7;
    };
    Register Value;
    VersionRegister() : Value(Read(Index::VERSION)) {}
} __attribute__((packed));

union LogicalDestinationRegister {
    struct {
        uint32_t : 24, LogicalId : 8;
    };
    Register Value;
    LogicalDestinationRegister() : Value(Read(Index::LOGICAL_DESTINATION)) {}
    ~LogicalDestinationRegister() { Write(Index::LOGICAL_DESTINATION, Value); }
} __attribute__((packed));

enum DestinationModel : uint32_t {
    CLUSTER = 0x0,
    FLAT = 0xf,
};

union DestinationFormatRegister {
    struct {
        uint32_t : 28;
        DestinationModel Model : 4;
    };
    Register Value;
    DestinationFormatRegister() : Value(Read(Index::DESTINATION_FORMAT)) {}
    ~DestinationFormatRegister() { Write(Index::DESTINATION_FORMAT, Value); }
} __attribute__((packed));

union TaskPriorityRegister {
    struct {
        uint32_t SubClass : 4, Priority : 4, : 24;
    };
    Register Value;
    TaskPriorityRegister() : Value(Read(Index::TASK_PRIORITY)) {}
    ~TaskPriorityRegister() { Write(Index::TASK_PRIORITY, Value); }
} __attribute__((packed));

union SpuriousInterruptVectorRegister {
    struct {
        uint32_t SpuriousVector : 8;
        uint32_t ApicEnabled : 1;
        uint32_t FocusProcessorCheckingDisabled : 1;
        uint32_t : 2;
        uint32_t EoiBroadcastSuppression : 1;
        uint32_t : 19;
    };
    Register Value;
    SpuriousInterruptVectorRegister() : Value(Read(Index::SPURIOUS_INTERRUPT_VECTOR)) {}
    ~SpuriousInterruptVectorRegister() { Write(Index::SPURIOUS_INTERRUPT_VECTOR, Value); }
} __attribute__((packed));
static_assert(sizeof(SpuriousInterruptVectorRegister) == 4, "LAPIC SVR has the wrong size");

uint8_t GetId() {
    return static_cast<uint8_t>(IdentificationRegister().ApicId);
}

uint8_t GetLogicalId() {
    return static_cast<uint8_t>(LogicalDestinationRegister().LogicalId);
}

uint8_t GetVersion() {
    return static_cast<uint8_t>(VersionRegister().Version);
}

void Initialize(uint8_t InLogicalId) {
    {
        LogicalDestinationRegister ldr;
        ldr.LogicalId = InLogicalId;
    }
    {
        // Flat model: the destination field is a bit mask of logical IDs.
        DestinationFormatRegister dfr;
        dfr.Model = DestinationModel::FLAT;
    }
    {
        // Priority 0 accepts every vector.
        TaskPriorityRegister tpr;
        tpr.Priority = 0;
        tpr.SubClass = 0;
    }
    {
        SpuriousInterruptVectorRegister svr;
        svr.SpuriousVector = 0xff;
        svr.ApicEnabled = 1;
        svr.FocusProcessorCheckingDisabled = 1;
    }
}

void EndOfInterrupt() {
    // Reading a register first flushes any posted write to the APIC, which some
    // chipsets need before the EOI is seen in the right order.
    Read(Index::SPURIOUS_INTERRUPT_VECTOR);
    Write(Index::END_OF_INTERRUPT, 0);
}

uint16_t GetInServiceVector() {
    // The 256 in-service bits live in eight registers, 16 bytes apart. Scan from
    // the top because the highest in-service vector is the active one.
    for (int word = 7; word >= 0; word--) {
        const Register bits = Read(static_cast<Index>(Index::IN_SERVICE + word * 0x10));
        if (bits != 0) {
            return static_cast<uint16_t>(word * 32 + (31 - __builtin_clz(bits)));
        }
    }
    return 0;
}

}  // namespace LocalApic
