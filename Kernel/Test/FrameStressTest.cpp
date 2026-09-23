#include "Test/Test.h"
#include "Interrupt/Guard.h"
#include "Memory/Frame.h"
#include "Memory/Paging.h"
#include "Sync/Semaphore.h"
#include "Thread/Thread.h"

namespace {

constexpr unsigned WORKER_COUNT = 3;
constexpr unsigned ROUNDS = 200;
constexpr size_t FRAMES_PER_ROUND = 4;

Semaphore s_Finished(0);
unsigned s_Aliased[WORKER_COUNT] = {};
unsigned s_Exhausted[WORKER_COUNT] = {};

uint64_t Token(unsigned InWorker, unsigned InRound, size_t InSlot) {
    return (uint64_t{InWorker} << 40) | (uint64_t{InRound} << 8) | InSlot;
}

class StressThread : public Thread {
public:
    explicit StressThread(unsigned InIndex) : m_Index(InIndex) {}

    void Action() override {
        for (unsigned round = 0; round < ROUNDS; round++) {
            RunRound(round);
        }

        Guarded guard = Guard::Enter();
        s_Finished.V(guard.Vault());
    }

private:
    void RunRound(unsigned InRound) {
        uintptr_t frames[FRAMES_PER_ROUND] = {};

        for (size_t slot = 0; slot < FRAMES_PER_ROUND; slot++) {
            frames[slot] = Frame::Allocate();
            if (frames[slot] == 0) {
                s_Exhausted[m_Index]++;
                continue;
            }
            *reinterpret_cast<volatile uint64_t*>(Paging::ToVirtual(frames[slot])) =
                Token(m_Index, InRound, slot);
        }

        for (size_t slot = 0; slot < FRAMES_PER_ROUND; slot++) {
            if (frames[slot] == 0) {
                continue;
            }
            if (*reinterpret_cast<volatile uint64_t*>(Paging::ToVirtual(frames[slot])) !=
                Token(m_Index, InRound, slot)) {
                s_Aliased[m_Index]++;
            }
            Frame::Free(frames[slot]);
        }
    }

    unsigned m_Index;
};

StressThread s_Workers[WORKER_COUNT] = {StressThread(0), StressThread(1), StressThread(2)};

}  // namespace

void Test::RunFrameStressSuite() {
    Begin("Frame stress");

    // Readying a thread maps it a stack, so the baseline has to come after.
    for (StressThread& worker : s_Workers) {
        worker.Prepare();
    }

    const size_t free = Frame::GetFreeFrames();

    {
        Guarded guard = Guard::Enter();
        for (StressThread& worker : s_Workers) {
            guard.Vault().Scheduler.Ready(&worker);
        }
    }

    for (unsigned index = 0; index < WORKER_COUNT; index++) {
        Guarded guard = Guard::Enter();
        s_Finished.P(guard.Vault());
    }

    unsigned aliased = 0;
    unsigned exhausted = 0;
    for (unsigned index = 0; index < WORKER_COUNT; index++) {
        aliased += s_Aliased[index];
        exhausted += s_Exhausted[index];
    }

    TEST_CHECK_EQ(aliased, 0u);
    TEST_CHECK_EQ(exhausted, 0u);
    TEST_CHECK_EQ(Frame::GetFreeFrames(), free);
}
