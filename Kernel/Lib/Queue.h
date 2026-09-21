#pragma once
#include "Types.h"

// Intrusive singly linked FIFO.
//
// The link pointer lives inside the element, so enqueuing never allocates and
// can therefore be done from an epilogue. The member to use is a template
// parameter, which lets one type sit in several unrelated queues:
//
//     Queue<Thread, &Thread::m_QueueLink> ReadyQueue;
//
// Not thread safe on its own -- every user holds the Guard lock or has
// interrupts disabled.
template <typename T, T* T::*LinkMember>
class Queue {
public:
    constexpr Queue() = default;

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    bool IsEmpty() const { return m_Head == nullptr; }
    T* First() const { return m_Head; }
    T* Last() const { return m_Tail; }

    static T* Next(const T& InItem) { return InItem.*LinkMember; }

    bool Contains(const T& InItem) const {
        return m_Head != nullptr && (InItem.*LinkMember != nullptr || m_Tail == &InItem);
    }

    /// Appends to the back. Returns false if the item is already queued here.
    bool Append(T& InItem) {
        if (Contains(InItem)) {
            return false;
        }
        InItem.*LinkMember = nullptr;
        if (m_Head == nullptr) {
            m_Head = &InItem;
        } else {
            m_Tail->*LinkMember = &InItem;
        }
        m_Tail = &InItem;
        return true;
    }

    /// Inserts at the front.
    bool Prepend(T& InItem) {
        if (Contains(InItem)) {
            return false;
        }
        InItem.*LinkMember = m_Head;
        m_Head = &InItem;
        if (m_Tail == nullptr) {
            m_Tail = &InItem;
        }
        return true;
    }

    /// Inserts directly behind InAfter, which must already be queued.
    bool InsertAfter(T& InAfter, T& InItem) {
        if (!Contains(InAfter) || Contains(InItem)) {
            return false;
        }
        if (&InAfter == m_Tail) {
            return Append(InItem);
        }
        InItem.*LinkMember = InAfter.*LinkMember;
        InAfter.*LinkMember = &InItem;
        return true;
    }

    /// Removes and returns the front element, or nullptr when empty.
    T* Dequeue() {
        T* item = m_Head;
        if (item != nullptr) {
            m_Head = item->*LinkMember;
            item->*LinkMember = nullptr;
            if (m_Head == nullptr) {
                m_Tail = nullptr;
            }
        }
        return item;
    }

    /// Unlinks an arbitrary element. Returns false if it was not queued.
    bool Remove(T& InItem) {
        if (!Contains(InItem)) {
            return false;
        }
        if (m_Head == &InItem) {
            Dequeue();
            return true;
        }
        for (T* current = m_Head; current != nullptr; current = current->*LinkMember) {
            if (current->*LinkMember == &InItem) {
                current->*LinkMember = InItem.*LinkMember;
                if (m_Tail == &InItem) {
                    m_Tail = current;
                }
                InItem.*LinkMember = nullptr;
                return true;
            }
        }
        return false;
    }

    class Iterator {
    public:
        explicit Iterator(T* InCurrent) : m_Current(InCurrent) {}
        T& operator*() const { return *m_Current; }
        T* operator->() const { return m_Current; }
        Iterator& operator++() {
            m_Current = m_Current->*LinkMember;
            return *this;
        }
        bool operator==(const Iterator& InOther) const { return m_Current == InOther.m_Current; }
        bool operator!=(const Iterator& InOther) const { return m_Current != InOther.m_Current; }

    private:
        T* m_Current;
    };

    Iterator begin() const { return Iterator(m_Head); }
    Iterator end() const { return Iterator(nullptr); }

private:
    T* m_Head = nullptr;
    T* m_Tail = nullptr;
};
