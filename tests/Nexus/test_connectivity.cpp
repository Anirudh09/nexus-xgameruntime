// SPDX-License-Identifier: LGPL-2.1-or-later
#include <gtest/gtest.h>
#include "../../src/System/Networking/Connectivity.h"

namespace
{
struct Adapter
{
    sockaddr_in address{}, gateway{};
    IP_ADAPTER_UNICAST_ADDRESS unicast{};
    IP_ADAPTER_GATEWAY_ADDRESS route{};
    IP_ADAPTER_ADDRESSES data{};
    Adapter()
    {
        address.sin_family = gateway.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl( 0xc0a80102 );
        gateway.sin_addr.s_addr = htonl( 0xc0a80101 );
        unicast.Address = { reinterpret_cast<sockaddr *>( &address ), sizeof( address ) };
        route.Address = { reinterpret_cast<sockaddr *>( &gateway ), sizeof( gateway ) };
        data.Length = sizeof( data );
        data.OperStatus = IfOperStatusUp;
        data.IfType = IF_TYPE_IEEE80211;
        data.FirstUnicastAddress = &unicast;
        data.FirstGatewayAddress = &route;
    }
};
using namespace nexus::networking;
}
TEST( NexusConnectivity, NoAdaptersMeansInitializedOfflineWithoutInventedEthernetOrCost )
{
    XNetworkingConnectivityHint hint{};
    ASSERT_EQ( MapConnectivity( nullptr, &hint ), S_OK );
    EXPECT_TRUE( hint.networkInitialized );
    EXPECT_EQ( hint.connectivityLevel, XNetworkingConnectivityLevelHint::None );
    EXPECT_EQ( hint.connectivityCost, XNetworkingConnectivityCostHint::Unknown );
    EXPECT_EQ( hint.ianaInterfaceType, 0 );
    EXPECT_EQ( MapConnectivity( nullptr, nullptr ), E_POINTER );
}
TEST( NexusConnectivity, DownAndLoopbackAdaptersCannotProduceInternetHint )
{
    Adapter adapter;
    XNetworkingConnectivityHint hint{};
    adapter.data.OperStatus = IfOperStatusDown;
    ASSERT_EQ( MapConnectivity( &adapter.data, &hint ), S_OK );
    EXPECT_EQ( hint.connectivityLevel, XNetworkingConnectivityLevelHint::None );
    adapter.data.OperStatus = IfOperStatusUp;
    adapter.data.IfType = IF_TYPE_SOFTWARE_LOOPBACK;
    ASSERT_EQ( MapConnectivity( &adapter.data, &hint ), S_OK );
    EXPECT_EQ( hint.connectivityLevel, XNetworkingConnectivityLevelHint::None );
}
TEST( NexusConnectivity, ActiveWifiWithoutGatewayIsLocal )
{
    Adapter adapter;
    adapter.data.FirstGatewayAddress = nullptr;
    XNetworkingConnectivityHint hint{};
    ASSERT_EQ( MapConnectivity( &adapter.data, &hint ), S_OK );
    EXPECT_EQ( hint.connectivityLevel, XNetworkingConnectivityLevelHint::LocalAccess );
    EXPECT_EQ( hint.ianaInterfaceType, IF_TYPE_IEEE80211 );
    EXPECT_EQ( hint.connectivityCost, XNetworkingConnectivityCostHint::Unknown );
}
TEST( NexusConnectivity, GatewayHintRetainsActualInterfaceAndUnknownCost )
{
    Adapter adapter;
    XNetworkingConnectivityHint hint{};
    ASSERT_EQ( MapConnectivity( &adapter.data, &hint ), S_OK );
    EXPECT_EQ( hint.connectivityLevel, XNetworkingConnectivityLevelHint::InternetAccess );
    EXPECT_EQ( hint.ianaInterfaceType, IF_TYPE_IEEE80211 );
    EXPECT_EQ( hint.connectivityCost, XNetworkingConnectivityCostHint::Unknown );
    EXPECT_FALSE( hint.roaming );
    EXPECT_FALSE( hint.overDataLimit );
    EXPECT_FALSE( hint.approachingDataLimit );
}
TEST( NexusConnectivity, GatewayWithoutUsableLocalAddressCannotImplyConnectivity )
{
    Adapter adapter;
    XNetworkingConnectivityHint hint{};
    for ( auto value : { 0u, 0x7f000001u, 0xe0000001u, 0xffffffffu } )
    {
        adapter.address.sin_addr.s_addr = htonl( value );
        ASSERT_EQ( MapConnectivity( &adapter.data, &hint ), S_OK );
        EXPECT_EQ( hint.connectivityLevel, XNetworkingConnectivityLevelHint::None );
    }
}
TEST( NexusConnectivity, MalformedGatewayDoesNotImplyInternet )
{
    Adapter adapter;
    XNetworkingConnectivityHint hint{};
    adapter.route.Address.iSockaddrLength = 1;
    ASSERT_EQ( MapConnectivity( &adapter.data, &hint ), S_OK );
    EXPECT_EQ( hint.connectivityLevel, XNetworkingConnectivityLevelHint::LocalAccess );
    adapter.route.Address.iSockaddrLength = sizeof( sockaddr_in );
    adapter.gateway.sin_addr.s_addr = 0;
    ASSERT_EQ( MapConnectivity( &adapter.data, &hint ), S_OK );
    EXPECT_EQ( hint.connectivityLevel, XNetworkingConnectivityLevelHint::LocalAccess );
}
TEST( NexusConnectivity, RoutableAdapterWinsOverAnUnroutedFirstAdapter )
{
    Adapter local, routed;
    XNetworkingConnectivityHint hint{};
    local.data.FirstGatewayAddress = nullptr;
    local.data.IfType = IF_TYPE_ETHERNET_CSMACD;
    local.data.Next = &routed.data;
    ASSERT_EQ( MapConnectivity( &local.data, &hint ), S_OK );
    EXPECT_EQ( hint.connectivityLevel, XNetworkingConnectivityLevelHint::InternetAccess );
    EXPECT_EQ( hint.ianaInterfaceType, IF_TYPE_IEEE80211 );
}
TEST( NexusConnectivity, ActualIpv6AddressesAreSupportedWithoutExternalProbe )
{
    Adapter adapter;
    XNetworkingConnectivityHint hint{};
    sockaddr_in6 address{}, gateway{};
    address.sin6_family = gateway.sin6_family = AF_INET6;
    address.sin6_addr.s6_addr[0] = 0x20;
    address.sin6_addr.s6_addr[1] = 1;
    gateway.sin6_addr.s6_addr[0] = 0xfe;
    gateway.sin6_addr.s6_addr[1] = 0x80;
    adapter.unicast.Address = { reinterpret_cast<sockaddr *>( &address ), sizeof( address ) };
    adapter.route.Address = { reinterpret_cast<sockaddr *>( &gateway ), sizeof( gateway ) };
    ASSERT_EQ( MapConnectivity( &adapter.data, &hint ), S_OK );
    EXPECT_EQ( hint.connectivityLevel, XNetworkingConnectivityLevelHint::InternetAccess );
    address.sin6_addr = {};
    address.sin6_addr.s6_addr[15] = 1;
    ASSERT_EQ( MapConnectivity( &adapter.data, &hint ), S_OK );
    EXPECT_EQ( hint.connectivityLevel, XNetworkingConnectivityLevelHint::None );
}
TEST( NexusConnectivity, CyclicListsFailWithClearedOutput )
{
    Adapter adapter;
    XNetworkingConnectivityHint hint{};
    adapter.data.Next = &adapter.data;
    EXPECT_EQ( MapConnectivity( &adapter.data, &hint ), HRESULT_FROM_WIN32( ERROR_INVALID_DATA ) );
    EXPECT_FALSE( hint.networkInitialized );
    EXPECT_EQ( hint.connectivityLevel, XNetworkingConnectivityLevelHint::Unknown );
    adapter.data.Next = nullptr;
    adapter.unicast.Next = &adapter.unicast;
    EXPECT_EQ( MapConnectivity( &adapter.data, &hint ), HRESULT_FROM_WIN32( ERROR_INVALID_DATA ) );
    adapter.unicast.Next = nullptr;
    adapter.route.Next = &adapter.route;
    EXPECT_EQ( MapConnectivity( &adapter.data, &hint ), HRESULT_FROM_WIN32( ERROR_INVALID_DATA ) );
}
