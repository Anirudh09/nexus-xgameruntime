// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <atomic>
#include <unknwn.h>

namespace nexus {
// A process-owned COM object retains one lifetime reference. Returned interface
// pointers own additional references, including successful QueryInterface calls.
template<class Interface, class... Aliases>
class StaticComObject : public Interface {
public:
    HRESULT WINAPI QueryInterface(REFIID iid, void** out) noexcept override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid != __uuidof(IUnknown) && iid != __uuidof(Interface) &&
            !(... || (iid == __uuidof(Aliases)))) return E_NOINTERFACE;
        *out = static_cast<Interface*>(this);
        AddRef();
        return S_OK;
    }
    ULONG WINAPI AddRef() noexcept override { return ++references_; }
    ULONG WINAPI Release() noexcept override { return --references_; }
private:
    std::atomic<ULONG> references_{1};
};
}
