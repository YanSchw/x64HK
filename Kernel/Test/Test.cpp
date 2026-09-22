#include "Test/Test.h"
#include "Arch/Cpu.h"
#include "Debug/DebugExit.h"
#include "Device/SerialStream.h"
#include "Thread/Thread.h"

namespace {

SerialStream s_Output;

unsigned s_Checks = 0;
unsigned s_Failures = 0;
unsigned s_Suites = 0;

/// Runs the suites that need to be on a thread, then reports for everyone.
class RunnerThread : public Thread {
public:
    void Action() override {
        Test::RunFrameStressSuite();
        Test::RunSemaphoreSuite();
        Test::RunGuardSuite();
        Test::Finish();
    }
};

RunnerThread s_Runner;

}  // namespace

OutputStream& Test::Out() {
    return s_Output;
}

void Test::Begin(const char* InName) {
    s_Suites++;
    Out() << "-- " << InName << EndLine << Flush;
}

void Test::Check(bool InPassed, const char* InExpression, const char* InFile, unsigned InLine) {
    s_Checks++;
    if (InPassed) {
        return;
    }

    s_Failures++;
    Out() << "   FAIL " << InFile << ':' << Dec << InLine << "  " << InExpression << EndLine << Flush;
}

void Test::RunUnitSuites() {
    Out() << EndLine << "x64HK tests" << EndLine << Flush;

    RunFrameSuite();
    RunFrameDrainSuite();
    RunHeapSuite();
    RunRingBufferSuite();
    RunKeyDecoderSuite();
    RunPs2ControllerSuite();
}

Thread& Test::Runner() {
    return s_Runner;
}

void Test::Finish() {
    const bool passed = s_Failures == 0;

    Out() << EndLine << (passed ? "PASS" : "FAIL") << ": " << Dec << s_Checks << " checks in " << s_Suites
          << " suites, " << s_Failures << " failed" << EndLine << Flush;

    // QEMU takes a moment to act on this, so anything printed after it would be
    // cut off mid word. The verdict above is the whole report.
    Debug::RequestExit(passed ? Debug::ExitCode::SUCCESS : Debug::ExitCode::FAILURE);
    Cpu::Die();
}
