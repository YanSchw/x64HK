#include "Test/Test.h"
#include "Arch/ControlRegister.h"
#include "Arch/LocalApic.h"
#include "Arch/Msr.h"
#include "Boot/Multiboot.h"
#include "Config.h"
#include "Memory/Frame.h"
#include "Memory/Heap.h"
#include "Memory/Paging.h"

extern "C" uint8_t ___KERNEL_TEXT_START___;
extern "C" uint8_t ___KERNEL_RODATA_START___;
extern "C" uint8_t ___KERNEL_DATA_START___;
extern "C" uint64_t KernelPageTableRoot;

namespace {

uintptr_t AddressOf(const uint8_t& InSymbol) {
    return reinterpret_cast<uintptr_t>(&InSymbol);
}

}  // namespace

void Test::RunPagingSuite() {
    Begin("Paging");

    // Running on the kernel's own tables, not the boot map.
    TEST_CHECK(KernelPageTableRoot != 0);
    TEST_CHECK_EQ(Cpu::CR3::Read() & ~uintptr_t{0xfff}, KernelPageTableRoot);

    // A read-only mapping does nothing in ring 0 without CR0.WP, and an NX bit
    // is a reserved bit without EFER.NXE.
    TEST_CHECK((Cpu::CR0::Read() & ToUnderlying(Cpu::CR0Flags::WP)) != 0);
    TEST_CHECK((Cpu::Msr<Cpu::MsrIndex::EFER>::Read() & ToUnderlying(Cpu::MsrEfer::NXE)) != 0);

    const uintptr_t text = AddressOf(___KERNEL_TEXT_START___);
    const uintptr_t rodata = AddressOf(___KERNEL_RODATA_START___);
    const uintptr_t data = AddressOf(___KERNEL_DATA_START___);

    // The kernel is linked into the higher half and loaded low.
    TEST_CHECK(text >= Config::KERNEL_VMA);
    TEST_CHECK_EQ(Paging::Translate(text), text - Config::KERNEL_VMA);
    TEST_CHECK_EQ(Paging::Translate(rodata), rodata - Config::KERNEL_VMA);
    TEST_CHECK_EQ(Paging::Translate(data), data - Config::KERNEL_VMA);

    // Video memory sits in a reserved hole the memory map never mentions.
    TEST_CHECK_EQ(Paging::Translate(0xb8000), uintptr_t{0xb8000});
    TEST_CHECK(Paging::IsWritable(0xb8000));

    // The application processors start here, with paging still off, but the
    // page has to be reachable for the trampoline to be copied in.
    TEST_CHECK_EQ(Paging::Translate(Config::AP_TRAMPOLINE_ADDRESS),
                  uintptr_t{Config::AP_TRAMPOLINE_ADDRESS});

    TEST_CHECK(Paging::IsExecutable(text));
    TEST_CHECK(!Paging::IsWritable(text));

    TEST_CHECK(!Paging::IsExecutable(rodata));
    TEST_CHECK(!Paging::IsWritable(rodata));

    TEST_CHECK(Paging::IsWritable(data));
    TEST_CHECK(!Paging::IsExecutable(data));

    // Anything the allocators hand out has to be mapped, writable and not
    // executable.
    const uintptr_t frame = Frame::Allocate();
    TEST_CHECK(frame != 0);
    TEST_CHECK_EQ(Paging::Translate(Paging::ToVirtual(frame)), frame);
    TEST_CHECK(Paging::IsWritable(Paging::ToVirtual(frame)));
    TEST_CHECK(!Paging::IsExecutable(Paging::ToVirtual(frame)));

    // The whole point of the direct map: RAM is not in the lower half any more,
    // which is what leaves that half to a user address space.
    TEST_CHECK_EQ(Paging::Translate(frame), uintptr_t{0});
    Frame::Free(frame);

    void* block = Heap::Allocate(64);
    const uintptr_t heap = reinterpret_cast<uintptr_t>(block);
    TEST_CHECK(block != nullptr);
    TEST_CHECK(heap >= Config::DIRECT_MAP_BASE);
    TEST_CHECK_EQ(Paging::Translate(heap), Paging::ToPhysical(heap));
    TEST_CHECK(Paging::IsWritable(heap));
    TEST_CHECK(!Paging::IsExecutable(heap));
    Heap::Free(block);

    const uintptr_t localApic = LocalApic::GetBaseAddress();
    TEST_CHECK_EQ(Paging::Translate(localApic), localApic);
    TEST_CHECK(Paging::IsWritable(localApic));
    TEST_CHECK(!Paging::IsExecutable(localApic));

    // Memory the boot map could not reach is mapped now.
    Multiboot::MemoryRegion region;
    for (unsigned index = 0; Multiboot::GetMemoryRegion(index, region); index++) {
        if (region.Type != Multiboot::MemoryType::AVAILABLE ||
            region.Address < Config::IDENTITY_MAPPED_LIMIT) {
            continue;
        }

        const uintptr_t high = Paging::ToVirtual(region.Address);
        TEST_CHECK_EQ(Paging::Translate(high), region.Address);
        TEST_CHECK(Paging::IsWritable(high));
        TEST_CHECK(!Paging::IsExecutable(high));
        break;
    }

    // Nothing is mapped where no memory was reported.
    constexpr uintptr_t BEYOND_ANY_MEMORY = 0x0000'7000'0000'0000;
    TEST_CHECK_EQ(Paging::Translate(BEYOND_ANY_MEMORY), uintptr_t{0});
    TEST_CHECK(!Paging::IsWritable(BEYOND_ANY_MEMORY));
}

#ifdef TEST_WPROTECT

void Test::RunWriteProtectSuite() {
    Begin("Write protect");
    Out() << "   writing into .text on purpose" << EndLine << Flush;

    *reinterpret_cast<volatile uint8_t*>(&___KERNEL_TEXT_START___) = 0x90;

    // Only reached if a read-only mapping did not stop a ring 0 write, which is
    // what CR0.WP is there to prevent.
    TEST_CHECK(false);
}

#endif
