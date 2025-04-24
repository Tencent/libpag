#pragma once

#include <atomic>
#include <iterator>
#include <map>
#include <vector>

namespace glbinding
{

template <typename T>
class RingBuffer 
{

public:
    // Buffer is limited to (maxValue(sizeType)/2 - 1) entries
    using SizeType = unsigned int;
    RingBuffer(SizeType maxSize);

    void resize(SizeType newSize);

    T nextHead(bool & available);
    bool push(T && entry);
    bool push(T & entry);

    using TailIdentifier = unsigned int;
    TailIdentifier addTail();
    void removeTail(TailIdentifier);
    const typename std::vector<T>::const_iterator cbegin(TailIdentifier key);
    bool valid(TailIdentifier key, const typename std::vector<T>::const_iterator & it);
    const typename std::vector<T>::const_iterator next(TailIdentifier key, const typename std::vector<T>::const_iterator & it);
    SizeType size(TailIdentifier);

    SizeType maxSize();
    SizeType size();
    bool isFull();
    bool isEmpty();

protected:
    SizeType next(SizeType current);
    bool isFull(SizeType nextHead);
    SizeType lastTail();
    SizeType size(SizeType, SizeType);

protected:
    std::vector<T> m_buffer;
    SizeType m_size;
    std::atomic<SizeType> m_head;
    std::map<TailIdentifier, std::atomic<SizeType>> m_tails;
};

} // namespace glbinding

#include "RingBuffer.hpp"
