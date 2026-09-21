#include "Debug/Assert.h"
#include "Debug/DebugExit.h"
#include "Debug/Output.h"
#include "Arch/Cpu.h"

[[noreturn]] void AssertionFailed(const char* InExpression, const char* InFunction, const char* InFile,
                                  int InLine) {
    DBG << "ASSERT failed: '" << InExpression << "' in " << InFunction << " at " << InFile << ":" << InLine
        << EndLine;
    Debug::RequestExit(Debug::ExitCode::FAILURE);
    Cpu::Die();
}
