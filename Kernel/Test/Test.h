#pragma once
#include "Types.h"
#include "Lib/OutputStream.h"

class Thread;

namespace Test {
OutputStream& Out();

/// Names the suite that the following checks belong to.
void Begin(const char* InName);

void Check(bool InPassed, const char* InExpression, const char* InFile, unsigned InLine);

template <typename ActualType, typename ExpectedType>
void CheckEqual(const ActualType& InActual, const ExpectedType& InExpected, const char* InExpression,
                const char* InFile, unsigned InLine) {
    const bool passed = (InActual == InExpected);
    Check(passed, InExpression, InFile, InLine);
    if (!passed) {
        Out() << "          actual " << InActual << ", expected " << InExpected << EndLine << Flush;
    }
}

/// Suites that need nothing but the heap. Run on the bootstrap core before the
/// scheduler exists.
void RunUnitSuites();

/// The thread that runs the suites needing a scheduler, then reports and asks
/// QEMU to exit. Ready it and it takes care of the rest.
Thread& Runner();

/// Prints the tally and exits the emulator. Never returns.
[[noreturn]] void Finish();

void RunFrameSuite();
void RunFrameDrainSuite();
void RunFrameStressSuite();
void RunHeapSuite();
void RunRingBufferSuite();
void RunKeyDecoderSuite();
void RunPs2ControllerSuite();
void RunSemaphoreSuite();
void RunGuardSuite();

}  // namespace Test

#define TEST_CHECK(EXPRESSION) Test::Check(!!(EXPRESSION), #EXPRESSION, __FILE__, __LINE__)

#define TEST_CHECK_EQ(ACTUAL, EXPECTED) \
    Test::CheckEqual((ACTUAL), (EXPECTED), #ACTUAL " == " #EXPECTED, __FILE__, __LINE__)
