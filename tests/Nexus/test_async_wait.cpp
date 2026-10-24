// SPDX-License-Identifier: MIT
#include "../test_fixtures.h"
#include <windows.h>
#include <xasync.h>
#include <xasyncprovider.h>
#include <thread>
#include <atomic>

TEST(NexusAsync, WaitIncludesDetachedCompletionAndCallbackCanReadOwnStatus) {
    const auto dll = LoadLibraryW(L"xgameruntime.dll");
    ASSERT_NE(dll, nullptr);
    using Query = HRESULT(WINAPI*)(REFCLSID, REFIID, void**);
    const auto query = reinterpret_cast<Query>(GetProcAddress(dll, "QueryApiImpl"));
    ASSERT_NE(query, nullptr);
    IXThreadingImpl* api = nullptr;
    ASSERT_EQ(query(CLSID_XThreadingImpl, __uuidof(IXThreadingImpl), reinterpret_cast<void**>(&api)), S_OK);
    XTaskQueueHandle queue = nullptr;
    ASSERT_EQ(api->XTaskQueueCreate(XTaskQueueDispatchMode::ThreadPool, XTaskQueueDispatchMode::ThreadPool, &queue), S_OK);
    struct Context {
        IXThreadingImpl* api;
        HANDLE entered, release, returned;
        std::atomic<HRESULT> status{E_UNEXPECTED};
    } context{api, CreateEventW(nullptr, TRUE, FALSE, nullptr), CreateEventW(nullptr, TRUE, FALSE, nullptr), CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    ASSERT_NE(context.entered, nullptr);
    ASSERT_NE(context.release, nullptr);
    ASSERT_NE(context.returned, nullptr);
    XAsyncBlock block{};
    block.queue = queue;
    block.context = &context;
    block.callback = [](XAsyncBlock* value) WINAPI {
        const auto test = static_cast<Context*>(value->context);
        EXPECT_EQ(test->api->XAsyncGetStatus(value, TRUE), E_ABORT);
        SetEvent(test->entered);
        EXPECT_EQ(WaitForSingleObject(test->release, 3000), WAIT_OBJECT_0);
    };
    const auto provider = [](XAsyncOp, const XAsyncProviderData*) WINAPI -> HRESULT { return S_OK; };
    ASSERT_EQ(api->XAsyncBegin(&block, nullptr, nullptr, "wait-contract", provider), S_OK);
    api->XAsyncComplete(&block, E_ABORT, 0);
    EXPECT_EQ(WaitForSingleObject(context.entered, 3000), WAIT_OBJECT_0);
    std::thread waiter([&] {
        context.status = api->XAsyncGetStatus(&block, TRUE);
        SetEvent(context.returned);
    });
    EXPECT_EQ(WaitForSingleObject(context.returned, 100), WAIT_TIMEOUT);
    SetEvent(context.release);
    waiter.join();
    EXPECT_EQ(context.status.load(), E_ABORT);
    EXPECT_EQ(api->XTaskQueueTerminate(queue, TRUE, nullptr, nullptr), S_OK);
    api->XTaskQueueCloseHandle(queue);
    api->Release();
    CloseHandle(context.entered);
    CloseHandle(context.release);
    CloseHandle(context.returned);
    FreeLibrary(dll);
}
