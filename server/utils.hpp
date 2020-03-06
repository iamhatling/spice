/*
   Copyright (C) 2019 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

/* Generic utilities for C++
 */
#pragma once

#include <memory>
#include <atomic>

#include "push-visibility.h"

namespace red {


template <typename T>
inline T* add_ref(T* p)
{
    if (p) {
        p->ref();
    }
    return p;
}


/* Smart pointer allocated once
 *
 * It just keep the pointer passed to constructor and delete
 * the object in the destructor. No copy or move allowed.
 * Very easy but make sure we don't change it and that's
 * initialized.
 */
template <typename T>
class unique_link
{
public:
    unique_link(): p(new T())
    {
    }
    unique_link(T* p): p(p)
    {
    }
    ~unique_link()
    {
        delete p;
    }
    T* operator->() noexcept
    {
        return p;
    }
    const T* operator->() const noexcept
    {
        return p;
    }
private:
    T *const p;
    unique_link(const unique_link&)=delete;
    void operator=(const unique_link&)=delete;
};


template <typename T>
struct GLibDeleter {
    void operator()(T* p)
    {
        g_free(p);
    }
};

template <typename T>
using glib_unique_ptr = std::unique_ptr<T, GLibDeleter<T>>;


/* Returns the size of an array.
 * Introduced in C++17 but lacking in C++11
 */
template <class T, size_t N>
constexpr size_t size(const T (&array)[N]) noexcept
{
    return N;
}


/* Basic shared pointer for internal reference
 *
 * Similar to STL version but using intrusive. This to allow creating multiple
 * shared pointers from the raw pointer without problems having the object
 * freed when one single set of shared pointer is removed. The code used
 * to increment the reference to make sure the object was not deleted in
 * some cases of self destruction.
 *
 * This class is inspired to boost::intrusive_ptr.
 *
 * To allow to reference and unrefered any object the object should
 * define shared_ptr_add_ref and shared_ptr_unref, both taking a pointer and incrementing and
 * decrementing respectively. You should not call these function yourselves.
 */
template <typename T>
class shared_ptr
{
public:
    explicit shared_ptr(T *p=nullptr): p(p)
    {
        if (p) {
            shared_ptr_add_ref(p);
        }
    }
    template <class Q>
    explicit shared_ptr(Q *p): shared_ptr(static_cast<T*>(p))
    {
    }
    shared_ptr(const shared_ptr& rhs): p(rhs.p)
    {
        if (p) {
            shared_ptr_add_ref(p);
        }
    }
    template <class Q>
    shared_ptr(const shared_ptr<Q>& rhs): shared_ptr(static_cast<T*>(rhs.get()))
    {
    }
    shared_ptr& operator=(const shared_ptr& rhs)
    {
        if (rhs.p != p) {
            reset(rhs.p);
        }
        return *this;
    }
    template <class Q>
    shared_ptr& operator=(const shared_ptr<Q>& rhs)
    {
        reset(rhs.get());
        return *this;
    }
    shared_ptr(shared_ptr&& rhs): p(rhs.p)
    {
        rhs.p = nullptr;
    }
    shared_ptr& operator=(shared_ptr&& rhs)
    {
        if (p) {
            shared_ptr_unref(p);
        }
        p = rhs.p;
        rhs.p = nullptr;
        return *this;
    }
    ~shared_ptr()
    {
        if (p) {
            shared_ptr_unref(p);
        }
    }
    void reset(T *ptr=nullptr)
    {
        if (ptr) {
            shared_ptr_add_ref(ptr);
        }
        if (p) {
            shared_ptr_unref(p);
        }
        p = ptr;
    }
    T *release()
    {
        T *ptr = p;
        if (p) {
            shared_ptr_unref(p);
            p = nullptr;
        }
        return ptr;
    }
    operator bool() const
    {
        return p;
    }
    T& operator*() const noexcept
    {
        return *p;
    }
    T* operator->() const noexcept
    {
        return p;
    }
    T *get() const noexcept
    {
        return p;
    }
private:
    T* p;
};

template <class T, class O>
inline bool operator==(const shared_ptr<T>& a, const shared_ptr<O>& b)
{
    return a.get() == b.get();
}

template <class T, class O>
inline bool operator!=(const shared_ptr<T>& a, const shared_ptr<O>& b)
{
    return a.get() != b.get();
}

/* Utility to help implementing shared_ptr requirement
 *
 * You should inherit publicly this class in order to have base internal reference counting
 * implementation.
 *
 * This class uses aromic operations and virtual destructor so it's not really light.
 */
class shared_ptr_counted
{
public:
    SPICE_CXX_GLIB_ALLOCATOR

    shared_ptr_counted(): ref_count(0)
    {
    }
protected:
    virtual ~shared_ptr_counted() {}
private:
    std::atomic_int ref_count;
    shared_ptr_counted(const shared_ptr_counted& rhs)=delete;
    void operator=(const shared_ptr_counted& rhs)=delete;
    friend inline void shared_ptr_add_ref(shared_ptr_counted*);
    friend inline void shared_ptr_unref(shared_ptr_counted*);
};

// implements requirements for shared_ptr
inline void shared_ptr_add_ref(shared_ptr_counted* p)
{
    ++p->ref_count;
}

inline void shared_ptr_unref(shared_ptr_counted* p)
{
    if (--p->ref_count == 0) {
        delete p;
    }
}


} // namespace red

#include "pop-visibility.h"
