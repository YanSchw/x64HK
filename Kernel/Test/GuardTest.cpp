#include "Test/Test.h"
#include "Arch/Cpu.h"
#include "Interrupt/Guard.h"
#include "Sync/Semaphore.h"
#include "Thread/Thread.h"

namespace {

constexpr unsigned INCREMENTS = 2000;
constexpr unsigned WORKER_COUNT = 2;

/// Deliberately not atomic: the Guard is the only thing keeping this coherent,
/// so a lost update means the critical section is not one.
uint64_t s_Shared = 0;

Semaphore s_Finished(0);

class IncrementThread : public Thread {
public:
    void Action() override {
        for (unsigned round = 0; round < INCREMENTS; round++) {
            Guarded guard = Guard::Enter();
            s_Shared++;
        }

        Guarded guard = Guard::Enter();
        s_Finished.V(guard.Vault());
    }
};

IncrementThread s_Workers[WORKER_COUNT];

}  // namespace

void Test::RunGuardSuite() {
    Begin("Guard");

    {
        Guarded guard = Guard::Enter();
        for (unsigned index = 0; index < WORKER_COUNT; index++) {
            guard.Vault().Scheduler.Ready(&s_Workers[index]);
        }
    }

    // The runner joins in, so the work is spread over as many cores as QEMU was
    // given. At -smp 1 this still exercises preemption mid critical section.
    for (unsigned round = 0; round < INCREMENTS; round++) {
        Guarded guard = Guard::Enter();
        s_Shared++;
    }

    for (unsigned index = 0; index < WORKER_COUNT; index++) {
        Guarded guard = Guard::Enter();
        s_Finished.P(guard.Vault());
    }

    {
        Guarded guard = Guard::Enter();
        TEST_CHECK_EQ(s_Shared, uint64_t{(WORKER_COUNT + 1) * INCREMENTS});
    }

    // Leaving the critical section has to restore the interrupt flag, not just
    // enable it: the epilogue drain runs with interrupts off in between.
    {
        TEST_CHECK(Cpu::Interrupt::IsEnabled());
        Guarded guard = Guard::Enter();
    }
    TEST_CHECK(Cpu::Interrupt::IsEnabled());
}
