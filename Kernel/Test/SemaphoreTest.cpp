#include "Test/Test.h"
#include "Interrupt/Guard.h"
#include "Sync/Semaphore.h"
#include "Thread/Thread.h"

namespace {

constexpr unsigned SIGNAL_COUNT = 200;

Semaphore s_Signal(0);
Semaphore s_Finished(0);

/// Hands SIGNAL_COUNT tokens over and then reports that it is done, so the
/// runner can tell "the producer finished" from "the producer is slow".
class SignalThread : public Thread {
public:
    void Action() override {
        for (unsigned round = 0; round < SIGNAL_COUNT; round++) {
            Guarded guard = Guard::Enter();
            s_Signal.V(guard.Vault());
        }

        Guarded guard = Guard::Enter();
        s_Finished.V(guard.Vault());
    }
};

SignalThread s_SignalThread;

}  // namespace

void Test::RunSemaphoreSuite() {
    Begin("Semaphore");

    {
        Guarded guard = Guard::Enter();
        TEST_CHECK_EQ(s_Signal.Count(), 0u);
        guard.Vault().Scheduler.Ready(&s_SignalThread);
    }

    // Every V() has to arrive exactly once. Too few and this blocks until the
    // harness times out; too many and the leftover count shows up below.
    unsigned received = 0;
    for (unsigned round = 0; round < SIGNAL_COUNT; round++) {
        Guarded guard = Guard::Enter();
        s_Signal.P(guard.Vault());
        received++;
    }
    TEST_CHECK_EQ(received, SIGNAL_COUNT);

    {
        Guarded guard = Guard::Enter();
        s_Finished.P(guard.Vault());
        TEST_CHECK_EQ(s_Signal.Count(), 0u);
    }

    // A counting semaphore that nobody is waiting on just accumulates.
    {
        Guarded guard = Guard::Enter();
        Vault& vault = guard.Vault();

        Semaphore counter(0);
        counter.V(vault);
        counter.V(vault);
        counter.V(vault);
        TEST_CHECK_EQ(counter.Count(), 3u);

        // These three cannot block, so they are safe to run inside the Guard.
        counter.P(vault);
        counter.P(vault);
        TEST_CHECK_EQ(counter.Count(), 1u);
        counter.P(vault);
        TEST_CHECK_EQ(counter.Count(), 0u);
    }

    {
        Guarded guard = Guard::Enter();
        Semaphore initialised(5);
        TEST_CHECK_EQ(initialised.Count(), 5u);
    }
}
