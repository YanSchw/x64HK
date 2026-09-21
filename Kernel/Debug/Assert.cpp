#include "Debug/Assert.h"
#include "Debug/Output.h"
#include "Arch/Cpu.h"

[[noreturn]] void AssertionFailed(const char* InExpression, const char* InFunction, const char* InFile,
                                  int InLine) {
    DBG << "ASSERT failed: '" << InExpression << "' in " << InFunction << " at " << InFile << ":" << InLine
        << EndLine;
    Cpu::Die();
}
