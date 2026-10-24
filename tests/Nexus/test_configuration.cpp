// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include "../../src/private.h"
#include "../../src/Runtime/Configuration.h"
#include <atomic>
#include <thread>
#include <vector>

TEST( NexusXml, CommentsCdataEntitiesAndUtf8 )
{
    nexus::XmlNode root;
    ASSERT_EQ( nexus::ParseXml( "<?xml version=\"1.0\"?><Game><!-- <Identity Name='Wrong'/> --><Value "
                                "attr='A&amp;B'>A&amp;B<![CDATA[<real>]]>č</Value></Game>",
                                root ),
               S_OK );
    EXPECT_EQ( root.name, "Game" );
    ASSERT_EQ( root.children.size(), 1u );
    EXPECT_EQ( root.children[0].text, "A&B<real>č" );
    EXPECT_EQ( root.children[0].attribute( "attr" ), "A&B" );
}
TEST( NexusXml, RejectsMalformedWithoutPartialPublication )
{
    for ( auto xml :
          { "", "<Game>", "<Game/><Game/>", "<Game><A></B></Game>", "<Game a='1' a='2'/>", "<Game>&missing;</Game>" } )
    {
        nexus::XmlNode root;
        root.name = "untouched";
        EXPECT_TRUE( FAILED( nexus::ParseXml( xml, root ) ) ) << xml;
        EXPECT_EQ( root.name, "untouched" );
    }
}
TEST( NexusXml, ProhibitsDtdAndExternalEntities )
{
    for ( auto xml : { "<!DOCTYPE Game [<!ENTITY x 'substitution'>]><Game>&x;</Game>",
                       "<!DOCTYPE Game SYSTEM 'http://127.0.0.1:9/no-network'><Game/>",
                       "<!DOCTYPE Game [<!ENTITY x SYSTEM 'file:///C:/no-private-files'>]><Game>&x;</Game>" } )
    {
        nexus::XmlNode root;
        EXPECT_TRUE( FAILED( nexus::ParseXml( xml, root ) ) );
    }
}
TEST( NexusXml, EnforcesDocumentAndTreeBounds )
{
    nexus::XmlNode root;
    EXPECT_TRUE( FAILED( nexus::ParseXml( std::string( 4 * 1024 * 1024 + 1, 'x' ), root ) ) );
    std::string deep;
    for ( int i = 0; i < 70; ++i )
        deep += "<A>";
    for ( int i = 0; i < 70; ++i )
        deep += "</A>";
    EXPECT_TRUE( FAILED( nexus::ParseXml( deep, root ) ) );
    std::string wide = "<Game>";
    for ( int i = 0; i < 16385; ++i )
        wide += "<A/>";
    wide += "</Game>";
    EXPECT_TRUE( FAILED( nexus::ParseXml( wide, root ) ) );
}
TEST( NexusXml, ConvertsUtf8WithoutLossOrSilentReplacement )
{
    std::wstring wide;
    std::string text;
    ASSERT_EQ( nexus::ToUtf16( "Příliš červený", wide ), S_OK );
    ASSERT_EQ( nexus::ToUtf8( wide, text ), S_OK );
    EXPECT_EQ( text, "Příliš červený" );
    EXPECT_TRUE( FAILED( nexus::ToUtf16( std::string_view( "\xff", 1 ), wide ) ) );
    EXPECT_TRUE( FAILED( nexus::ToUtf8( std::wstring( 1, 0xd800 ), text ) ) );
}
TEST( NexusConfiguration, DerivesRealPackageFamilyWithoutIdentitySubstitution )
{
    std::string result;
    ASSERT_EQ( nexus::GetPackageFamilyName(
                   "Microsoft.WindowsCalculator",
                   "CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US", result ),
               S_OK );
    EXPECT_EQ( result, "Microsoft.WindowsCalculator_8wekyb3d8bbwe" );
    std::string second;
    ASSERT_EQ( nexus::GetPackageFamilyName( "Microsoft.WindowsCalculator", "CN=DifferentPublisher", second ), S_OK );
    EXPECT_NE( result, second );
    for ( auto name : { "", "..", "../elsewhere", "C:\\elsewhere", "directory/file", "a:b" } )
        EXPECT_TRUE( FAILED( nexus::GetPackageFamilyName( name, "CN=Publisher", result ) ) );
}
TEST( NexusConfiguration, FailedInitializationIsRetryableThenPublishesAtomically )
{
    ASSERT_EQ( nexus::GetGameConfiguration(), nullptr );
    const char *invalid[] = {
        "<invalid>",
        "<Wrong/>",
        "<Game><TitleId>not-hex</TitleId></Game>",
        "<Game><Identity Name='../bad' Publisher='CN=A'/></Game>",
        "<Game><RelatedProducts><RelatedProduct>invalid</RelatedProduct></RelatedProducts></Game>",
        "<!DOCTYPE Game [<!ENTITY x 'x'>]><Game>&x;</Game>" };
    for ( auto xml : invalid )
    {
        INITIALIZE_OPTIONS options{ 1, TRUE, xml };
        EXPECT_EQ( nexus::InitializeGameConfiguration( &options ), E_GAMERUNTIME_GAMECONFIG_BAD_FORMAT );
        EXPECT_EQ( nexus::GetGameConfiguration(), nullptr );
    }
    INITIALIZE_OPTIONS missing{ 1, TRUE, nullptr };
    EXPECT_EQ( nexus::InitializeGameConfiguration( &missing ), E_GAMERUNTIME_GAMECONFIG_BAD_FORMAT );
    ASSERT_TRUE( SetEnvironmentVariableA( "NEXUS_RELATED_PRODUCT_IDS", "9AAAAAAAAAAA,9BBBBBBBBBBB,9AAAAAAAAAAA" ) );
    const char *xml =
        "<Game><Identity Name='Nexus.Test' Publisher='CN=Publisher'/><TitleId> 0x1234\n</TitleId>"
        "<MSAAppId>real-app-input</MSAAppId><StoreId>9CCCCCCCCCCC</StoreId><MSAFullTrust>true</MSAFullTrust>"
        "<RelatedProducts><RelatedProduct>9AAAAAAAAAAA</RelatedProduct></RelatedProducts></Game>";
    INITIALIZE_OPTIONS options{ 1, TRUE, xml };
    std::atomic<int> failed{ 0 };
    std::vector<std::thread> threads;
    for ( int i = 0; i < 12; ++i )
        threads.emplace_back(
            [&]
            {
                if ( nexus::InitializeGameConfiguration( &options ) != S_OK ) ++failed;
                const auto *current = nexus::GetGameConfiguration();
                if ( !current || current->titleId != 0x1234 || current->msaAppId != "real-app-input" ||
                     current->relatedProducts.size() != 2 )
                    ++failed;
            } );
    for ( auto &thread : threads )
        thread.join();
    ASSERT_EQ( failed.load(), 0 );
    const auto *current = nexus::GetGameConfiguration();
    ASSERT_NE( current, nullptr );
    EXPECT_EQ( current->storeId, "9CCCCCCCCCCC" );
    EXPECT_TRUE( current->fullTrust );
    EXPECT_TRUE( current->packageFamilyName.starts_with( "Nexus.Test_" ) );
    EXPECT_EQ( current->relatedProducts[1], "9BBBBBBBBBBB" );
    EXPECT_EQ( nexus::InitializeGameConfiguration( &missing ), S_OK );
    EXPECT_EQ( nexus::GetGameConfiguration(), current );
    SetEnvironmentVariableA( "NEXUS_RELATED_PRODUCT_IDS", nullptr );
}
