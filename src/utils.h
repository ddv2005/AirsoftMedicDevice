#pragma once
#include <queue>
#include "concurrency/Lock.h"
#include "concurrency/LockGuard.h"

template<class T>
class cProtectedQueue:public std::queue<T>
{
protected:
    concurrency::Lock m_lock;
    uint16_t m_max;
public:
    cProtectedQueue(uint16_t _max)
    {
        m_max = _max;
    }

    void lock()
    {
        m_lock.lock();
    }

    void unlock()
    {
        m_lock.unlock();
    }

    void push_item(const T& item)
    {
        concurrency::LockGuard lock(&m_lock);
        std::queue<T>::push(item);
        while(std::queue<T>::size()>m_max)
            std::queue<T>::pop();
    }

};

template<class T>
constexpr const T& clamp( const T& v, const T& lo, const T& hi ) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

