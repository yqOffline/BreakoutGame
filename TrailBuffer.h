#ifndef TRAIL_BUFFER_H
#define TRAIL_BUFFER_H

#include "raylib.h"
#include <vector>

class TrailBuffer {
public:
    explicit TrailBuffer(int maxLength = 10) 
        : m_maxLen(maxLength), m_head(0), m_size(0) {
        m_buffer.resize(m_maxLen);
    }

    void push(const Vector2& pos) {
        m_buffer[m_head] = pos;
        m_head = (m_head + 1) % m_maxLen;
        if (m_size < m_maxLen) ++m_size;
    }

    void clear() { m_size = 0; }
    int size() const { return m_size; }

    Vector2 operator[](int i) const {
        if (i < 0 || i >= m_size) return {0,0};
        int idx = (m_head - m_size + i + m_maxLen) % m_maxLen;
        return m_buffer[idx];
    }

private:
    int m_maxLen;
    int m_head;
    int m_size;
    std::vector<Vector2> m_buffer;
};

#endif