#include "Test/Test.h"
#include "Config.h"
#include "Memory/KernelStack.h"
#include "Memory/Paging.h"

void Test::RunKernelStackSuite() {
    Begin("KernelStack");

    const uintptr_t top = KernelStack::Allocate();
    const uintptr_t bottom = top - Config::THREAD_STACK_SIZE;
    const uintptr_t guard = bottom - Paging::PAGE_SIZE;

    TEST_CHECK(top > Config::KERNEL_STACK_BASE);
    TEST_CHECK_EQ(top % Paging::PAGE_SIZE, uintptr_t{0});

    TEST_CHECK(Paging::Translate(bottom) != 0);
    TEST_CHECK(Paging::Translate(top - 8) != 0);
    TEST_CHECK(Paging::IsWritable(bottom));
    TEST_CHECK(!Paging::IsExecutable(bottom));

    unsigned corrupted = 0;
    for (uintptr_t offset = 0; offset < Config::THREAD_STACK_SIZE; offset += 8) {
        *reinterpret_cast<volatile uint64_t*>(bottom + offset) = bottom + offset;
    }
    for (uintptr_t offset = 0; offset < Config::THREAD_STACK_SIZE; offset += 8) {
        if (*reinterpret_cast<volatile uint64_t*>(bottom + offset) != bottom + offset) {
            corrupted++;
        }
    }
    TEST_CHECK_EQ(corrupted, 0u);

    // Nothing at all is mapped below the stack, which is what turns an overflow
    // into a fault on the instruction that caused it.
    TEST_CHECK_EQ(Paging::Translate(guard), uintptr_t{0});
    TEST_CHECK(!Paging::IsWritable(guard));

    TEST_CHECK(KernelStack::IsGuardPage(guard));
    TEST_CHECK(KernelStack::IsGuardPage(guard + Paging::PAGE_SIZE - 1));
    TEST_CHECK(!KernelStack::IsGuardPage(bottom));
    TEST_CHECK(!KernelStack::IsGuardPage(top - 1));
    TEST_CHECK(!KernelStack::IsGuardPage(0));
    TEST_CHECK(!KernelStack::IsGuardPage(Config::KERNEL_STACK_BASE - 1));

    // Stacks do not overlap, and each one keeps its own guard.
    const uintptr_t second = KernelStack::Allocate();
    TEST_CHECK(second >= top + Paging::PAGE_SIZE + Config::THREAD_STACK_SIZE);
    TEST_CHECK(KernelStack::IsGuardPage(second - Config::THREAD_STACK_SIZE - Paging::PAGE_SIZE));
}

#ifdef TEST_OVERFLOW

namespace {

// Volatile so the compiler cannot prove the recursion never ends and turn it
// into something that does not actually consume stack.
volatile bool s_KeepGoing = true;

[[gnu::noinline]] void Recurse() {
    volatile uint8_t frame[512];
    frame[0] = 1;
    if (s_KeepGoing) {
        Recurse();
    }
    frame[1] = frame[0];
}

}  // namespace

void Test::RunStackOverflowSuite() {
    Begin("Stack overflow");
    Out() << "   running off the end of a thread stack on purpose" << EndLine << Flush;
    Recurse();
    TEST_CHECK(false);
}

#endif
