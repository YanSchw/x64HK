#pragma once
#include "Types.h"
#include "Config.h"
#include "Arch/Cache.h"
#include "Arch/Cpu.h"

// One instance of T per CPU core, each on its own cache line so that cores do
// not ping-pong a shared line between their L1 caches (false sharing).
template <typename T>
class PerCore {
public:
    constexpr PerCore() = default;

    explicit PerCore(const T& InInitial) {
        for (unsigned i = 0; i < Config::MAX_CORES; i++) {
            m_Instances[i].Value = InInitial;
        }
    }

    PerCore(const PerCore&) = delete;
    PerCore& operator=(const PerCore&) = delete;

    T& Get() { return m_Instances[Cpu::GetId()].Value; }
    const T& Get() const { return m_Instances[Cpu::GetId()].Value; }

    void Set(const T& InValue) { m_Instances[Cpu::GetId()].Value = InValue; }

    T& operator[](unsigned InCore) { return m_Instances[InCore].Value; }
    const T& operator[](unsigned InCore) const { return m_Instances[InCore].Value; }

    T* operator->() { return &Get(); }
    const T* operator->() const { return &Get(); }

    T& operator*() { return Get(); }
    const T& operator*() const { return Get(); }

private:
    struct CACHE_ALIGNED Slot {
        T Value{};
    };

    Slot m_Instances[Config::MAX_CORES] = {};
};
