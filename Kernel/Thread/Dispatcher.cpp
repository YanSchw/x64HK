#include "Thread/Dispatcher.h"
#include "Debug/Assert.h"

void Dispatcher::Go(Thread* InFirst) {
    ASSERT(InFirst != nullptr);
    m_ActiveThread.Set(InFirst);
    InFirst->Go();
    __builtin_unreachable();
}

void Dispatcher::Dispatch(Thread* InNext) {
    Thread* current = Active();
    ASSERT(InNext != nullptr);
    ASSERT(current != nullptr);

    if (current == InNext) {
        return;
    }

    m_ActiveThread.Set(InNext);
    current->Resume(InNext);
}
