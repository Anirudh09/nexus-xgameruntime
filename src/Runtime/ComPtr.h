// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <utility>

namespace nexus
{
template<class T> class ComPtr
{
    T *value_ = nullptr;

  public:
    ComPtr() = default;
    explicit ComPtr( T *value ) : value_( value ) {}
    ~ComPtr()
    {
        if ( value_ ) value_->Release();
    }
    ComPtr( const ComPtr & ) = delete;
    ComPtr &operator=( const ComPtr & ) = delete;
    ComPtr( ComPtr &&other ) noexcept : value_( std::exchange( other.value_, nullptr ) ) {}
    T *operator->() const noexcept
    {
        return value_;
    }
    T *get() const noexcept
    {
        return value_;
    }
    T **put() noexcept
    {
        if ( value_ ) value_->Release();
        value_ = nullptr;
        return &value_;
    }
};
}
