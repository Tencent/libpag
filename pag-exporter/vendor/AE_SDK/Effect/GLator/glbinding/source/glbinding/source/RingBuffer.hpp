#pragma once

#include <iomanip>
#include <thread>
#include <iterator>
#include <cassert>
#include <cmath>

#include "RingBuffer.h"

namespace glbinding
{

template <typename T>
RingBuffer<T>::RingBuffer(const unsigned int maxSize)
:   m_size{maxSize+1}
,   m_head{0}
{
    m_buffer.resize(m_size);
}

template <typename T>
void RingBuffer<T>::resize(const unsigned int newSize)
{
    m_size = newSize + 1;
    m_buffer.resize(m_size);
}

template <typename T>
T RingBuffer<T>::nextHead(bool & available)
{
    auto head = m_head.load(std::memory_order_relaxed);
    auto nextHead = next(head);

    if (isFull(nextHead))
    {
        available = false;
        return nullptr;
    }

    available = true;
    return m_buffer[nextHead];
}

template <typename T>
bool RingBuffer<T>::push(T && entry)
{
    auto head = m_head.load(std::memory_order_relaxed);
    auto nextHead = next(head);

    if (isFull(nextHead))
    {
        return false;
    }

    assert(head < m_size);

    if (m_buffer.size() <= head)
    {
        // This should never happen because m_buffer is reserving m_size
        m_buffer.push_back(entry);
    }
    else
        m_buffer[head] = entry;

    m_head.store(nextHead, std::memory_order_release);

    return true;
}

template <typename T>
bool RingBuffer<T>::push(T & entry)
{
    auto head = m_head.load(std::memory_order_relaxed);
    auto nextHead = next(head);

    if (isFull(nextHead))
    {
        return false;
    }

    assert(head < m_size);

    if (m_buffer.size() <= head)
    {
        m_buffer.push_back(entry);
    }
    else
    {
        m_buffer[head] = entry;
    }

    m_head.store(nextHead, std::memory_order_release);

    return true;
}

template <typename T>
typename RingBuffer<T>::TailIdentifier RingBuffer<T>::addTail()
{
    auto i = TailIdentifier{0};

    while(m_tails.find(i) != m_tails.end())
    {
        ++i;
    }

    m_tails[i] = m_head.load(std::memory_order_acquire);

    return i;
}

template <typename T>
void RingBuffer<T>::removeTail(TailIdentifier key)
{
    m_tails.erase(key);
}

template <typename T>
const typename std::vector<T>::const_iterator RingBuffer<T>::cbegin(TailIdentifier key)
{
    auto tail = m_tails[key].load(std::memory_order_relaxed);
    auto i = m_buffer.cbegin();

    std::advance(i, tail);

    return i;
}

template <typename T>
bool RingBuffer<T>::valid(TailIdentifier /*key*/, const typename std::vector<T>::const_iterator & it)
{
    auto pos = std::abs(std::distance(m_buffer.cbegin(), it));
    auto head = m_head.load(std::memory_order_acquire);

    return (static_cast<SizeType>(pos) != head);
}

template <typename T>
const typename std::vector<T>::const_iterator RingBuffer<T>::next(TailIdentifier key, const typename std::vector<T>::const_iterator & it)
{
    auto tail = m_tails[key].load(std::memory_order_acquire);

    if (tail == m_head.load(std::memory_order_acquire))
    {
        return it;
    }

    auto nextTail = next(tail);

    m_tails[key].store(nextTail, std::memory_order_release);

    return cbegin(key);
}

template <typename T>
typename RingBuffer<T>::SizeType RingBuffer<T>::size(TailIdentifier key)
{
    auto head = m_head.load(std::memory_order_acquire);
    auto tail = m_tails[key].load(std::memory_order_acquire);

    return size(head, tail);
}

template <typename T>
typename RingBuffer<T>::SizeType RingBuffer<T>::maxSize()
{
    return m_size - 1;
}

template <typename T>
typename RingBuffer<T>::SizeType RingBuffer<T>::size()
{
    auto head = m_head.load(std::memory_order_acquire);
    auto tail = lastTail();

    return size(head, tail);
}

template <typename T>
bool RingBuffer<T>::isFull()
{
    auto head = m_head.load(std::memory_order_relaxed);
    auto nextHead = next(head);

    return isFull(nextHead);
}

template <typename T>
bool RingBuffer<T>::isEmpty()
{
    auto tail = lastTail();

    return tail == m_head.load(std::memory_order_acquire);
}


//protected

template <typename T>
typename RingBuffer<T>::SizeType RingBuffer<T>::next(SizeType current)
{
    return (current + 1) % m_size;
}

template <typename T>
bool RingBuffer<T>::isFull(SizeType nextHead)
{
    for (auto it = m_tails.cbegin(); it != m_tails.cend(); ++it)
    {
        auto tailPos = it->second.load(std::memory_order_acquire);

        if (nextHead == tailPos)
            return true;
    }

    return false;
}

template <typename T>
typename RingBuffer<T>::SizeType RingBuffer<T>::lastTail()
{
    auto head = m_head.load(std::memory_order_relaxed);
    auto last = head + m_size;

    for (auto it = m_tails.cbegin(); it != m_tails.cend(); ++it)
    {
        auto tailPos = it->second.load(std::memory_order_acquire);
        if (tailPos <= head)
            tailPos += m_size;

        if (tailPos < last)
            last = tailPos;
    }

    return last % m_size;
}

template <typename T>
typename RingBuffer<T>::SizeType RingBuffer<T>::size(SizeType head, SizeType tail)
{
    if (head < tail)
    {
        return m_size - tail + head;
    }
    else
    {
        return head - tail;
    }
}

} // namespace glbinding
