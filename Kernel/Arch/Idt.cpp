#include "Arch/Idt.h"
#include "Arch/Gdt.h"

namespace Idt {

alignas(16) constinit static InterruptDescriptor s_Table[Cpu::Interrupt::VECTOR_COUNT] = {};

struct Register {
    uint16_t Limit = sizeof(s_Table) - 1;
    InterruptDescriptor* Base = s_Table;
} __attribute__((packed));
static_assert(sizeof(Register) == 10, "Idt::Register has the wrong size");

void Set(Cpu::Interrupt::Vector InVector, InterruptDescriptor InDescriptor) {
    s_Table[ToUnderlying(InVector)] = InDescriptor;
}

void Load() {
    const Register idtr;
    asm volatile("lidt %0" : : "m"(idtr) : "memory");
}

}  // namespace Idt
