#include "Test/Test.h"
#include "Lib/RingBuffer.h"

void Test::RunRingBufferSuite() {
    Begin("RingBuffer");

    // One slot is always left empty so full and empty stay distinguishable, so
    // four slots hold three values.
    RingBuffer<int, 4> buffer;
    TEST_CHECK_EQ(buffer.Capacity(), 3u);
    TEST_CHECK(buffer.IsEmpty());
    TEST_CHECK_EQ(buffer.Count(), 0u);

    // A failed Consume leaves the caller's variable alone.
    int value = -1;
    TEST_CHECK(!buffer.Consume(value));
    TEST_CHECK_EQ(value, -1);

    TEST_CHECK(buffer.Produce(1));
    TEST_CHECK(buffer.Produce(2));
    TEST_CHECK(buffer.Produce(3));
    TEST_CHECK_EQ(buffer.Count(), 3u);
    TEST_CHECK(!buffer.IsEmpty());

    // Full rejects rather than overwriting the oldest entry: a dropped key is
    // better than a key that changes into another one.
    TEST_CHECK(!buffer.Produce(4));
    TEST_CHECK_EQ(buffer.Count(), 3u);

    TEST_CHECK(buffer.Consume(value));
    TEST_CHECK_EQ(value, 1);
    TEST_CHECK(buffer.Consume(value));
    TEST_CHECK_EQ(value, 2);
    TEST_CHECK(buffer.Consume(value));
    TEST_CHECK_EQ(value, 3);
    TEST_CHECK(buffer.IsEmpty());
    TEST_CHECK(!buffer.Consume(value));

    // Walking the indices past the modulus many times over: Count() has to stay
    // right once the write index has wrapped behind the read index.
    unsigned failures = 0;
    for (int round = 0; round < 1000; round++) {
        if (!buffer.Produce(round) || !buffer.Produce(round + 1)) {
            failures++;
            continue;
        }
        if (buffer.Count() != 2) {
            failures++;
        }
        if (!buffer.Consume(value) || value != round) {
            failures++;
        }
        if (!buffer.Consume(value) || value != round + 1) {
            failures++;
        }
    }
    TEST_CHECK_EQ(failures, 0u);
    TEST_CHECK(buffer.IsEmpty());

    // Filling and draining repeatedly must not drift the indices apart.
    failures = 0;
    for (int round = 0; round < 100; round++) {
        while (buffer.Produce(round)) {
        }
        if (buffer.Count() != buffer.Capacity()) {
            failures++;
        }
        while (buffer.Consume(value)) {
        }
        if (buffer.Count() != 0 || !buffer.IsEmpty()) {
            failures++;
        }
    }
    TEST_CHECK_EQ(failures, 0u);
}
