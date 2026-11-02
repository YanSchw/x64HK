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

void RunRound(unsigned InWorker, unsigned InRound) {
    uintptr_t frames[FRAMES_PER_ROUND] = {};

    for (size_t slot = 0; slot < FRAMES_PER_ROUND; slot++) {
        frames[slot] = Frame::Allocate();
        if (frames[slot] == 0) {
            s_Exhausted[InWorker]++;
            continue;
        }
        *reinterpret_cast<volatile uint64_t*>(Paging::ToVirtual(frames[slot])) =
            Token(InWorker, InRound, slot);
    }

    for (size_t slot = 0; slot < FRAMES_PER_ROUND; slot++) {
        if (frames[slot] == 0) {
            continue;
        }
        if (*reinterpret_cast<volatile uint64_t*>(Paging::ToVirtual(frames[slot])) !=
            Token(InWorker, InRound, slot)) {
            s_Aliased[InWorker]++;
        }
        Frame::Free(frames[slot]);
    }
}

void Stress(void* InIndex) {
    const unsigned index = *static_cast<const unsigned*>(InIndex);

    for (unsigned round = 0; round < ROUNDS; round++) {
        RunRound(index, round);
    }

    Guarded guard = Guard::Enter();
    s_Finished.V(guard.Vault());
}

unsigned s_Index[WORKER_COUNT] = {0, 1, 2};

Thread s_Workers[WORKER_COUNT] = {
    {"frame stress", Stress, &s_Index[0]},
    {"frame stress", Stress, &s_Index[1]},
    {"frame stress", Stress, &s_Index[2]},
};

}  // namespace

void Test::RunFrameStressSuite() {
    Begin("Frame stress");

    // Readying a thread maps it a stack, so the baseline has to come after.
    for (Thread& worker : s_Workers) {
        worker.Prepare();
    }

    const size_t free = Frame::GetFreeFrames();

    {
        Guarded guard = Guard::Enter();
        for (Thread& worker : s_Workers) {
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
