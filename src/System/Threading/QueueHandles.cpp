// SPDX-License-Identifier: LGPL-2.1-or-later
// Queue lifetime adaptation for the upstream C++ engine. Public opaque tokens
// are never dereferenced or reused. Closing removes exactly one owned reference;
// operations already in flight retain their own references outside the lock.
#include "QueueHandles.h"
#include <limits>
#include <unordered_map>

namespace nexus
{
namespace
{
struct Entry
{
    TaskQueueImpl *queue;
    bool internal;
    SIZE_T owned_references;
};
struct Registry
{
    std::mutex lock;
    std::unordered_map<XTaskQueueHandle, Entry> entries;
    uintptr_t next = 1;
};
Registry &registry()
{
    static Registry value;
    return value;
}
}

HRESULT register_queue_object( TaskQueueImpl *queue ) noexcept
{
    auto &state = registry();
    std::lock_guard<std::mutex> guard( state.lock );
    try
    {
        return state.entries.emplace( queue->GetHandle(), Entry{ queue, true, 0 } ).second ? S_OK : E_UNEXPECTED;
    }
    catch ( const std::bad_alloc & )
    {
        return E_OUTOFMEMORY;
    }
}

void unregister_queue_object( TaskQueueImpl *queue ) noexcept
{
    auto &state = registry();
    std::lock_guard<std::mutex> guard( state.lock );
    // Destruction cannot race a successful acquire: acquiring first increments
    // a nonzero reference count while this entry and its storage are protected.
    state.entries.erase( queue->GetHandle() );
}

referenced_ptr<ITaskQueue> acquire_queue( XTaskQueueHandle handle ) noexcept
{
    auto &state = registry();
    std::lock_guard<std::mutex> guard( state.lock );
    const auto found = state.entries.find( handle );
    if ( found == state.entries.end() || !found->second.queue->TryAddRef() ) return {};
    return referenced_ptr<ITaskQueue>::adopt( found->second.queue );
}

HRESULT create_queue_handle( ITaskQueue *queue, XTaskQueueHandle *out ) noexcept
{
    if ( !out ) return E_POINTER;
    *out = nullptr;
    auto &state = registry();
    std::lock_guard<std::mutex> guard( state.lock );
    // Odd opaque tokens cannot collide with the aligned internal object header.
    // Stop rather than wrap and make a closed handle refer to a new queue.
    if ( state.next >= std::numeric_limits<uintptr_t>::max() - 2 ) return E_OUTOFMEMORY;
    const auto handle = reinterpret_cast<XTaskQueueHandle>( state.next );
    state.next += 2;
    try
    {
        if ( !state.entries.emplace( handle, Entry{ static_cast<TaskQueueImpl *>( queue ), false, 1 } ).second )
            return E_UNEXPECTED;
    }
    catch ( const std::bad_alloc & )
    {
        return E_OUTOFMEMORY;
    }
    queue->AddRef();
    *out = handle;
    return S_OK;
}

HRESULT reference_queue_handle( ITaskQueue *queue, XTaskQueueHandle *out ) noexcept
{
    if ( !out ) return E_POINTER;
    *out = nullptr;
    auto &state = registry();
    std::lock_guard<std::mutex> guard( state.lock );
    const auto found = state.entries.find( queue->GetHandle() );
    if ( found == state.entries.end() ) return E_GAMERUNTIME_INVALID_HANDLE;
    if ( found->second.owned_references == std::numeric_limits<SIZE_T>::max() ) return E_OUTOFMEMORY;
    ++found->second.owned_references;
    queue->AddRef();
    *out = found->first;
    return S_OK;
}

void close_queue_handle( XTaskQueueHandle handle ) noexcept
{
    TaskQueueImpl *queue = nullptr;
    auto &state = registry();
    {
        std::lock_guard<std::mutex> guard( state.lock );
        const auto found = state.entries.find( handle );
        if ( found == state.entries.end() || !found->second.owned_references ) return;
        queue = found->second.queue;
        if ( found->second.internal )
            --found->second.owned_references;
        else
            state.entries.erase( found );
    }
    // Destruction and queue callbacks may re-enter the registry.
    queue->Release();
}
}
