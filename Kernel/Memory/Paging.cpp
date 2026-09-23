#include "Memory/Paging.h"
#include "Arch/Apic.h"
#include "Arch/ControlRegister.h"
#include "Arch/CpuId.h"
#include "Arch/LocalApic.h"
#include "Arch/Msr.h"
#include "Boot/Multiboot.h"
#include "Config.h"
#include "Debug/Assert.h"
#include "Debug/Output.h"
#include "Debug/Panic.h"
#include "Lib/Math.h"
#include "Arch/CpuInterrupt.h"
#include "Lib/String.h"
#include "Memory/Frame.h"
#include "Sync/SpinLock.h"

extern "C" uint8_t ___BOOT_START___;
extern "C" uint8_t ___BOOT_END___;
extern "C" uint8_t ___KERNEL_TEXT_START___;
extern "C" uint8_t ___KERNEL_TEXT_END___;
extern "C" uint8_t ___KERNEL_RODATA_START___;
extern "C" uint8_t ___KERNEL_RODATA_END___;
extern "C" uint8_t ___KERNEL_DATA_START___;
extern "C" uint8_t ___KERNEL_DATA_END___;

/// Read by Boot/LongMode.asm. While it is zero every core builds the boot map;
/// once the kernel has its own tables, later cores load these instead.
extern "C" uint64_t KernelPageTableRoot = 0;

namespace Paging {

constexpr uint64_t PRESENT = 1ULL << 0;
constexpr uint64_t WRITABLE = 1ULL << 1;
constexpr uint64_t CACHE_DISABLE = 1ULL << 4;
constexpr uint64_t LARGE = 1ULL << 7;
constexpr uint64_t NO_EXECUTE = 1ULL << 63;
constexpr uint64_t ADDRESS_MASK = 0x000f'ffff'ffff'f000ULL;

constexpr unsigned TOP_LEVEL = 4;
constexpr unsigned LEAF_LEVEL = 1;
constexpr size_t ENTRIES_PER_TABLE = 512;

/// Everything below this is mapped whole: the BIOS data area, video memory and
/// the page the application processors start on are not in the memory map, but
/// the kernel still has to reach them.
constexpr uintptr_t LOW_MEMORY_END = 2 * MIB;

static uintptr_t s_Root = 0;
static SpinLock s_Lock;
static bool s_NoExecuteAvailable = false;
static size_t s_TableCount = 0;
static uint64_t s_MappedRamEnd = 0;

static size_t IndexAt(uintptr_t InVirtual, unsigned InLevel) {
    return (InVirtual >> (12 + 9 * (InLevel - 1))) & (ENTRIES_PER_TABLE - 1);
}

static uint64_t* TableAt(uintptr_t InPhysical) {
    return reinterpret_cast<uint64_t*>(ToVirtual(InPhysical));
}

/// Returns the frame, not a pointer: a table entry holds a physical address.
static uintptr_t AllocateTable() {
    const uintptr_t frame = Frame::Allocate();
    if (frame == 0) {
        return 0;
    }
    memset(TableAt(frame), 0, PAGE_SIZE);
    s_TableCount++;
    return frame;
}

/// Walks down to InLevel, creating the tables it passes through.
static uint64_t* Descend(uintptr_t InVirtual, unsigned InLevel) {
    uint64_t* table = TableAt(s_Root);

    for (unsigned level = TOP_LEVEL; level > InLevel; level--) {
        uint64_t& entry = table[IndexAt(InVirtual, level)];

        if ((entry & PRESENT) == 0) {
            const uintptr_t created = AllocateTable();
            if (created == 0) {
                return nullptr;
            }
            // Permissions are the AND of every level, so anything but the leaf
            // has to stay permissive.
            entry = created | PRESENT | WRITABLE;
        }

        table = TableAt(entry & ADDRESS_MASK);
    }

    return table;
}

static uint64_t FlagsFor(Access InAccess, Caching InCaching) {
    uint64_t flags = PRESENT;

    if (InAccess == Access::READ_WRITE) {
        flags |= WRITABLE;
    }
    if (InAccess != Access::READ_EXECUTE && s_NoExecuteAvailable) {
        flags |= NO_EXECUTE;
    }
    if (InCaching == Caching::DISABLED) {
        flags |= CACHE_DISABLE;
    }

    return flags;
}

static const uint64_t* FindEntry(uintptr_t InVirtual) {
    const uint64_t* table = TableAt(s_Root);

    for (unsigned level = TOP_LEVEL; level > LEAF_LEVEL; level--) {
        const uint64_t entry = table[IndexAt(InVirtual, level)];
        if ((entry & PRESENT) == 0 || (entry & LARGE) != 0) {
            return nullptr;
        }
        table = TableAt(entry & ADDRESS_MASK);
    }

    const uint64_t* entry = &table[IndexAt(InVirtual, LEAF_LEVEL)];
    return (*entry & PRESENT) != 0 ? entry : nullptr;
}

bool Map(uintptr_t InVirtual, uintptr_t InPhysical, size_t InSize, Access InAccess,
         Caching InCaching) {
    // Stacks are mapped long after boot, from whichever core needs one.
    Cpu::Interrupt::Guard interruptGuard;
    SpinLock::Scope lockGuard(s_Lock);

    const uint64_t flags = FlagsFor(InAccess, InCaching);
    const uintptr_t end = InVirtual + InSize;

    uintptr_t source = InPhysical & ~(PAGE_SIZE - 1);

    for (uintptr_t page = InVirtual & ~(PAGE_SIZE - 1); page < end; page += PAGE_SIZE) {
        uint64_t* table = Descend(page, LEAF_LEVEL);
        if (table == nullptr) {
            return false;
        }

        table[IndexAt(page, LEAF_LEVEL)] = source | flags;
        source += PAGE_SIZE;
    }

    return true;
}

uintptr_t Translate(uintptr_t InVirtual) {
    const uint64_t* entry = FindEntry(InVirtual);
    if (entry == nullptr) {
        return 0;
    }
    return (*entry & ADDRESS_MASK) | (InVirtual & (PAGE_SIZE - 1));
}

bool IsWritable(uintptr_t InVirtual) {
    const uint64_t* entry = FindEntry(InVirtual);
    return entry != nullptr && (*entry & WRITABLE) != 0;
}

bool IsExecutable(uintptr_t InVirtual) {
    const uint64_t* entry = FindEntry(InVirtual);
    return entry != nullptr && (!s_NoExecuteAvailable || (*entry & NO_EXECUTE) == 0);
}

static void MapIdentity(uintptr_t InStart, uintptr_t InEnd, Access InAccess,
                        Caching InCaching = Caching::NORMAL) {
    if (InEnd <= InStart) {
        return;
    }
    if (!Map(InStart, InStart, InEnd - InStart, InAccess, InCaching)) {
        PANIC("Not enough memory for the kernel page tables");
    }
}

/// All of RAM, in the higher half. Nothing puts it in the lower half any more,
/// which is what leaves that half free for a user address space.
static void MapDirectMap() {
    Multiboot::MemoryRegion region;
    for (unsigned index = 0; Multiboot::GetMemoryRegion(index, region); index++) {
        if (region.Type != Multiboot::MemoryType::AVAILABLE) {
            continue;
        }

        if (!Map(ToVirtual(region.Address), region.Address, region.Length, Access::READ_WRITE)) {
            PANIC("Not enough memory for the kernel page tables");
        }
        s_MappedRamEnd = Math::Max(s_MappedRamEnd, region.End());
    }
}

/// The kernel is linked into the higher half but loaded low, so its virtual and
/// physical addresses differ by a constant.
static void MapKernelSpan(const uint8_t& InStart, const uint8_t& InEnd, Access InAccess) {
    const uintptr_t start = reinterpret_cast<uintptr_t>(&InStart);
    const uintptr_t end = reinterpret_cast<uintptr_t>(&InEnd);
    if (end <= start) {
        return;
    }
    if (!Map(start, start - Config::KERNEL_VMA, end - start, InAccess)) {
        PANIC("Not enough memory for the kernel page tables");
    }
}

/// Mapped after the bulk of memory, so these permissions win.
static void MapKernelImage() {
    // The 32-bit boot path runs from its load address, and the application
    // processors come back through it long after these tables are live.
    MapIdentity(reinterpret_cast<uintptr_t>(&___BOOT_START___),
                reinterpret_cast<uintptr_t>(&___BOOT_END___), Access::READ_EXECUTE);

    MapKernelSpan(___KERNEL_TEXT_START___, ___KERNEL_TEXT_END___, Access::READ_EXECUTE);
    MapKernelSpan(___KERNEL_RODATA_START___, ___KERNEL_RODATA_END___, Access::READ_ONLY);
    MapKernelSpan(___KERNEL_DATA_START___, ___KERNEL_DATA_END___, Access::READ_WRITE);
}

static void MapDeviceRegisters() {
    MapIdentity(LocalApic::GetBaseAddress(), LocalApic::GetBaseAddress() + PAGE_SIZE,
                Access::READ_WRITE, Caching::DISABLED);

    const uintptr_t ioApic = Apic::GetIoApicAddress();
    if (ioApic != 0) {
        MapIdentity(ioApic, ioApic + PAGE_SIZE, Access::READ_WRITE, Caching::DISABLED);
    }
}

static void EnableNoExecute() {
    using Efer = Cpu::Msr<Cpu::MsrIndex::EFER>;
    Efer::Write(Efer::Read() | ToUnderlying(Cpu::MsrEfer::NXE));
}

static void Activate() {
    // Without this, a read-only mapping does not stop ring 0 from writing to it
    // and the whole W^X split would be advisory.
    Cpu::CR0::Write(Cpu::CR0::Read() | ToUnderlying(Cpu::CR0Flags::WP));
    Cpu::CR3::Write(s_Root);
}

void Initialize() {
    s_NoExecuteAvailable = Cpu::Id::Has(Cpu::Id::ExtendedFeatureEdx::NX);
    if (s_NoExecuteAvailable) {
        EnableNoExecute();
    }

    s_Root = AllocateTable();
    if (s_Root == 0) {
        PANIC("No memory for the kernel page tables");
    }

    // The first megabyte and the device registers keep their one to one
    // mapping: real mode starts the application processors there, and the
    // framebuffer and APICs are addressed by their hardware addresses.
    MapIdentity(0, LOW_MEMORY_END, Access::READ_WRITE);
    MapDeviceRegisters();

    MapDirectMap();
    MapKernelImage();

    // Boot/LongMode.asm loads this with a 32-bit move.
    ASSERT(s_Root < 4 * GIB);
    KernelPageTableRoot = s_Root;

    Activate();

    // Every frame is reachable now, including the ones the boot map could not
    // address.
    Frame::SetReachableLimit(s_MappedRamEnd);
}

void Dump() {
    DBG << "Paging: " << Dec << s_TableCount << " tables, " << s_TableCount * PAGE_SIZE / KIB
        << " KiB, no-execute " << (s_NoExecuteAvailable ? "on" : "unavailable") << EndLine;
}

}  // namespace Paging
