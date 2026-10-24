// SPDX-License-Identifier: MIT
#include "../test_fixtures.h"
#include <windows.h>
#define WIDL_EXPLICIT_AGGREGATE_RETURNS
#include <xsystem.h>
#include <cstring>

TEST(NexusSystem, DesktopAnalyticsWithoutWinRTApartment) {
    const auto dll = LoadLibraryW(L"xgameruntime.dll");
    ASSERT_NE(dll, nullptr);
    using Query = HRESULT(WINAPI*)(REFCLSID, REFIID, void**);
    const auto query = reinterpret_cast<Query>(GetProcAddress(dll, "QueryApiImpl"));
    ASSERT_NE(query, nullptr);
    IXSystemAnalyticsImpl* analytics = nullptr;
    ASSERT_EQ(query(CLSID_XSystemAnalyticsImpl, __uuidof(IXSystemAnalyticsImpl), reinterpret_cast<void**>(&analytics)), S_OK);
    XSystemAnalyticsInfo data{};
    EXPECT_EQ(analytics->XSystemGetAnalyticsInfo(&data), &data);
    EXPECT_EQ(std::strcmp(data.family, "Windows.Desktop"), 0);
    EXPECT_EQ(std::strcmp(data.form, "Desktop"), 0);
    EXPECT_GT(data.osVersion.major, 0);
    EXPECT_EQ(data.osVersion.major, data.hostingOsVersion.major);
    EXPECT_EQ(data.osVersion.build, data.hostingOsVersion.build);
    EXPECT_EQ(data.osVersion.revision, 0);
    analytics->Release();
    FreeLibrary(dll);
}

TEST(NexusSystem, QueryRejectsNullAndUnknownWithoutStaleOutput) {
    const auto dll = LoadLibraryW(L"xgameruntime.dll");
    ASSERT_NE(dll, nullptr);
    using Query = HRESULT(WINAPI*)(REFCLSID, REFIID, void**);
    const auto query = reinterpret_cast<Query>(GetProcAddress(dll, "QueryApiImpl"));
    ASSERT_NE(query, nullptr);
    EXPECT_EQ(query(CLSID_XSystemImpl, __uuidof(IXSystemImpl), nullptr), E_POINTER);
    void* value = reinterpret_cast<void*>(1);
    const GUID absent{0x12345678, 0, 0, {}};
    EXPECT_EQ(query(CLSID_XSystemImpl, absent, &value), E_NOINTERFACE);
    EXPECT_EQ(value, nullptr);
    value = reinterpret_cast<void*>(1);
    EXPECT_EQ(query(absent, __uuidof(IUnknown), &value), REGDB_E_CLASSNOTREG);
    EXPECT_EQ(value, nullptr);
    FreeLibrary(dll);
}
