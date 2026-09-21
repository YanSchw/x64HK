#pragma once
#include "Types.h"

// Collects single characters and hands them over in blocks. Emitting one
// character at a time is expensive for every backend we have (I/O ports on the
// serial line, a lock plus scrolling on the screen), so output is batched until
// the buffer fills up or someone asks for a flush.
class StringBuffer {
public:
    virtual ~StringBuffer() = default;

    StringBuffer(const StringBuffer&) = delete;
    StringBuffer& operator=(const StringBuffer&) = delete;

protected:
    static constexpr size_t BUFFER_SIZE = 128;

    constexpr StringBuffer() = default;

    void Put(char InChar) {
        if (m_Position == BUFFER_SIZE) {
            Flush();
        }
        m_Buffer[m_Position++] = InChar;
    }

    /// Hands m_Buffer[0 .. m_Position) to the backend and resets m_Position.
    virtual void Flush() = 0;

    /// Null terminates the pending characters in place and returns them. The
    /// extra slot below means this never has to drop anything.
    const char* Terminated() {
        m_Buffer[m_Position] = '\0';
        return m_Buffer;
    }

    /// One slot past BUFFER_SIZE so Terminated() always has room.
    char m_Buffer[BUFFER_SIZE + 1] = {};
    size_t m_Position = 0;
};
