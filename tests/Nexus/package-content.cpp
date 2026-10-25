/* SPDX-License-Identifier: MIT
 * Account-free fixtures and real IXPackage ABI. No account, license or save API.
 * Disk installation records never establish ownership. */
#define INITGUID
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/System/XPackage.cpp"
#include <xstore.h>

static unsigned checks, failures, visits, expected;
static BOOL stop_early, recurse;
static WCHAR folder[MAX_PATH];
static const char *main_config = "<Game><Identity Name=\"Nexus.Base\" Publisher=\"CN=Probe\"/>"
                                 "<StoreId>B00000000001</StoreId><RelatedProducts><RelatedProduct>B00000000002</"
                                 "RelatedProduct></RelatedProducts></Game>";
#define CHECK( name, test )                                                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        ++checks;                                                                                                      \
        if ( !( test ) )                                                                                               \
        {                                                                                                              \
            ++failures;                                                                                                \
            printf( "FAIL %s line=%d\n", name, __LINE__ );                                                             \
        }                                                                                                              \
    } while ( 0 )

static void write_text( const WCHAR *path, const char *text )
{
    HANDLE file = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    DWORD written;
    CHECK( "fixture-open", file != INVALID_HANDLE_VALUE );
    if ( file == INVALID_HANDLE_VALUE ) return;
    CHECK( "fixture-write", WriteFile( file, text, strlen( text ), &written, NULL ) && written == strlen( text ) );
    CloseHandle( file );
}

static void configure( const char *text )
{
    INITIALIZE_OPTIONS options{};
    options.isInlineConfig = TRUE;
    options.gameConfig = text;
    CHECK( "config-xml", nexus::InitializeGameConfiguration( &options ) == S_OK );
}

static void fixture( const WCHAR *name, const char *identity, const char *id, const char *parent, const char *parent_id,
                     UINT guid, BOOL game )
{
    WCHAR root[MAX_PATH], file[MAX_PATH], layout[80];
    char config[4096], manifest[4096], dependency[512] = "", allowed[256] = "";
    content_join( folder, name, root );
    CreateDirectoryW( root, NULL );
    if ( !game )
    {
        snprintf( dependency, sizeof( dependency ), "<MainPackageDependency Name=\"%s\"/>", parent );
        snprintf( allowed, sizeof( allowed ), "<AllowedProducts><AllowedProduct>%s</AllowedProduct></AllowedProducts>",
                  parent_id );
    }
    snprintf( config, sizeof( config ),
              "<Game><Identity Name=\"%s\" Publisher=\"CN=Probe\" Version=\"1.2.3.4\"/>"
              "<StoreId>%s</StoreId><ShellVisuals DefaultDisplayName=\"Campaign &amp; More\" "
              "Description=\"Description\" PublisherDisplayName=\"Publisher\"/>"
              "%s<DesktopRegistration>%s</DesktopRegistration>%s</Game>",
              identity, id, allowed, dependency, game ? "<Executables/>" : "" );
    snprintf( manifest, sizeof( manifest ),
              "<Package xmlns:uap4=\"urn:probe\"><Identity Name=\"%s\" Publisher=\"CN=Probe\" Version=\"1.2.3.4\"/>"
              "<Dependencies>%s</Dependencies>%s</Package>",
              identity, dependency, game ? "<Applications/>" : "" );
    content_join( root, L"MicrosoftGame.Config", file );
    write_text( file, config );
    content_join( root, L"appxmanifest.xml", file );
    write_text( file, manifest );
    swprintf( layout, 80, L"layout_%08x-1111-2222-3333-444444444444.xml", guid );
    content_join( root, layout, file );
    write_text( file,
                "<Package><Chunk Id=\"Registration\"><FileGroup SourcePath=\".\" Include=\"MicrosoftGame.Config\"/>"
                "<FileGroup SourcePath=\".\" Include=\"*.png\"/></Chunk></Package>" );
    content_join( root, L"logo.png", file );
    write_text( file, "synthetic payload, not a game" );
}

static BOOLEAN WINAPI visit( void *context, const XPackageDetails *details )
{
    ( void )context;
    CHECK( "record-kind", details->kind == XPackageKind::Content );
    CHECK( "record-installed", !details->installing );
    CHECK( "record-index", details->index == visits );
    CHECK( "record-count", details->count == expected );
    CHECK( "record-store", content_id( details->storeId ) );
    CHECK( "record-identifier", strlen( details->packageIdentifier ) == 32 );
    CHECK( "record-version", details->version.major == 1 && details->version.minor == 2 &&
                                 details->version.build == 3 && details->version.revision == 4 );
    CHECK( "record-xml-entities", !strcmp( details->displayName, "Campaign & More" ) );
    visits++;
    if ( recurse )
    {
        unsigned saved = visits;
        BOOL saved_stop = stop_early;
        recurse = FALSE;
        visits = 0;
        stop_early = TRUE;
        CHECK( "reentrant-enumerate",
               enumerate_content_packages( XPackageEnumerationScope::ThisOnly, NULL, visit ) == S_OK && visits == 1 );
        visits = saved;
        stop_early = saved_stop;
    }
    return !stop_early;
}

static void enumerate( XPackageEnumerationScope scope, unsigned count, BOOL stop )
{
    expected = count;
    visits = 0;
    stop_early = stop;
    CHECK( "enumerate-success", enumerate_content_packages( scope, NULL, visit ) == S_OK );
    CHECK( "enumerate-count", visits == ( stop && count ? 1 : count ) );
}

static void helpers( void )
{
    XVersion version;
    char identifier[33];
    WCHAR path[MAX_PATH], file[MAX_PATH];
    struct content_package *package =
        static_cast<std::remove_reference_t<decltype( package )>>( calloc( 1, sizeof( *package ) ) );
    CHECK( "store-id-valid", content_id( "B00000000001" ) );
    CHECK( "store-id-invalid", !content_id( "b00000000001" ) && !content_id( "invalid" ) );
    CHECK( "version-valid", content_version( "65535.2.3.4", &version ) && version.major == 65535 );
    CHECK( "version-overflow", !content_version( "65536.0.0.0", &version ) );
    CHECK( "version-trailing", !content_version( "1.2.3.4x", &version ) );
    CHECK( "version-short", !content_version( "1.2.3", &version ) );
    CHECK( "layout-guid", content_layout_id( L"layout_12345678-1234-5678-9abc-123456789abc.xml", identifier ) &&
                              strlen( identifier ) == 32 );
    CHECK( "layout-guid-invalid", !content_layout_id( L"layout_bad.xml", identifier ) );
    CHECK( "source-traversal", !content_relative( "../escape", FALSE ) && !content_relative( "a/../escape", TRUE ) );
    CHECK( "source-absolute", !content_relative( "C:\\escape", FALSE ) && !content_relative( "/escape", FALSE ) );
    CHECK( "source-relative", content_relative( ".", FALSE ) && content_relative( "*.png", TRUE ) );
    fixture( L"own-one", "Nexus.Dlc.One", "D00000000001", "Nexus.Base", "B00000000001", 1, FALSE );
    fixture( L"own-two", "Nexus.Dlc.Two", "D00000000002", "Nexus.Base", "B00000000001", 2, FALSE );
    fixture( L"related-base", "Nexus.Related", "B00000000002", NULL, NULL, 3, TRUE );
    fixture( L"related-dlc", "Nexus.Related.Dlc", "D00000000003", "Nexus.Related", "B00000000002", 4, FALSE );
    fixture( L"other-base", "Nexus.Other", "B00000000003", NULL, NULL, 5, TRUE );
    fixture( L"other-dlc", "Nexus.Other.Dlc", "D00000000004", "Nexus.Other", "B00000000003", 6, FALSE );
    fixture( L"orphan", "Nexus.Orphan", "D00000000005", "Nexus.Missing", "B00000000004", 7, FALSE );
    configure( main_config );
    SetEnvironmentVariableW( L"XODUS_CONTENT_ROOTS", folder );
    enumerate( XPackageEnumerationScope::ThisOnly, 2, FALSE );
    enumerate( XPackageEnumerationScope::ThisAndRelated, 3, FALSE );
    enumerate( XPackageEnumerationScope::ThisPublisher, 4, FALSE );
    enumerate( XPackageEnumerationScope::ThisOnly, 2, TRUE );
    CHECK( "lookup-installed", content_query_identifier( "D00000000001", sizeof( identifier ), identifier ) == S_OK &&
                                   !strcmp( identifier, "00000001111122223333444444444444" ) );
    CHECK( "lookup-related", content_query_identifier( "D00000000003", sizeof( identifier ), identifier ) == S_OK &&
                                 !strcmp( identifier, "00000004111122223333444444444444" ) );
    CHECK( "lookup-unknown",
           content_query_identifier( "D00000000099", sizeof( identifier ), identifier ) == E_INVALIDARG &&
               !identifier[0] );
    CHECK( "lookup-unrelated",
           content_query_identifier( "D00000000004", sizeof( identifier ), identifier ) == E_INVALIDARG &&
               !identifier[0] );
    CHECK( "lookup-null-store", content_query_identifier( NULL, sizeof( identifier ), identifier ) == E_INVALIDARG );
    CHECK( "lookup-null-output",
           content_query_identifier( "D00000000001", sizeof( identifier ), NULL ) == E_INVALIDARG );
    CHECK( "lookup-short", content_query_identifier( "D00000000001", sizeof( identifier ) - 1, identifier ) ==
                                   E_NOT_SUFFICIENT_BUFFER &&
                               !identifier[0] );
    CHECK( "reverse-installed", xpackage_query_content_store( "00000001111122223333444444444444", sizeof( identifier ),
                                                              identifier ) == S_OK &&
                                    !strcmp( identifier, "D00000000001" ) );
    CHECK( "reverse-missing", xpackage_query_content_store( "00000099111122223333444444444444", sizeof( identifier ),
                                                            identifier ) == E_GAMEPACKAGE_NO_PACKAGE_IDENTIFIER &&
                                  !identifier[0] );
    CHECK( "reverse-invalid", xpackage_query_content_store( "bad", sizeof( identifier ), identifier ) == E_INVALIDARG );
    CHECK( "reverse-short", xpackage_query_content_store( "00000001111122223333444444444444", 12, identifier ) ==
                                E_NOT_SUFFICIENT_BUFFER );
    fixture( L"ambiguous", "Nexus.Dlc.Duplicate", "D00000000001", "Nexus.Base", "B00000000001", 8, FALSE );
    CHECK( "lookup-ambiguous",
           content_query_identifier( "D00000000001", sizeof( identifier ), identifier ) == E_INVALIDARG &&
               !identifier[0] );
    content_join( folder, L"ambiguous", path );
    content_join( path, L"MicrosoftGame.Config", file );
    DeleteFileW( file );
    recurse = TRUE;
    enumerate( XPackageEnumerationScope::ThisOnly, 2, TRUE );
    content_join( folder, L"own-one", path );
    CHECK( "valid-real-shape", content_load( path, package ) && package->content );
    content_join( path, L"logo.png", file );
    DeleteFileW( file );
    CHECK( "missing-payload-rejected", !content_load( path, package ) );
    write_text( file, "payload" );
    content_join( path, L"appxmanifest.xml", file );
    write_text( file, "<Package><Identity Name=\"Wrong\"/></Package>" );
    CHECK( "identity-mismatch-rejected", !content_load( path, package ) );
    fixture( L"own-one", "Nexus.Dlc.One", "D00000000001", "Nexus.Base", "B00000000001", 1, FALSE );
    content_join( path, L"MicrosoftGame.Config", file );
    write_text( file, "<!DOCTYPE Game [<!ENTITY fake 'bad'>]><Game>&fake;</Game>" );
    CHECK( "dtd-rejected", !content_load( path, package ) );
    fixture( L"own-one", "Nexus.Dlc.One", "D00000000001", "Nexus.Base", "B00000000001", 1, FALSE );
    /* Same physical package reachable through two roots must enumerate once. */
    {
        WCHAR roots[MAX_PATH * 2 + 2];
        swprintf( roots, ARRAY_SIZE( roots ), L"%ls;%ls", folder, path );
        SetEnvironmentVariableW( L"XODUS_CONTENT_ROOTS", roots );
        enumerate( XPackageEnumerationScope::ThisOnly, 2, FALSE );
    }
    SetEnvironmentVariableW( L"XODUS_CONTENT_ROOTS", L"relative/path" );
    CHECK( "relative-root-rejected",
           enumerate_content_packages( XPackageEnumerationScope::ThisOnly, NULL, visit ) == E_INVALIDARG );
    SetEnvironmentVariableW( L"XODUS_CONTENT_ROOTS", folder );
    free( package );
}

static void chunk_order( IXPackageImpl *package, const char *id, HRESULT installed )
{
    XPackageChunkSelector selectors[4]{}, before[4], bad{};
    selectors[0].type = XPackageChunkSelectorType::Chunk;
    selectors[0].chunkId = 123;
    selectors[1].type = XPackageChunkSelectorType::Language;
    selectors[1].language = "en-US;fr-FR";
    selectors[2].type = XPackageChunkSelectorType::Tag;
    selectors[2].tag = "Campaign";
    selectors[3].type = XPackageChunkSelectorType::Feature;
    selectors[3].feature = "Campaign";
    memcpy( before, selectors, sizeof( before ) );
    CHECK( "chunks-installed-selection",
           ( package )->XPackageChangeChunkInstallOrder( id, 4, selectors ) == installed );
    CHECK( "chunks-zero-selectors", ( package )->XPackageChangeChunkInstallOrder( id, 0, NULL ) == installed );
    CHECK( "chunks-no-input-mutation", !memcmp( before, selectors, sizeof( before ) ) );
    CHECK( "chunks-null-array", ( package )->XPackageChangeChunkInstallOrder( id, 1, NULL ) == E_INVALIDARG );
    CHECK( "chunks-null-id", ( package )->XPackageChangeChunkInstallOrder( NULL, 4, selectors ) == E_INVALIDARG );
    CHECK( "chunks-empty-id", ( package )->XPackageChangeChunkInstallOrder( "", 3, selectors ) == E_INVALIDARG );
    CHECK( "chunks-malformed-id",
           ( package )->XPackageChangeChunkInstallOrder( "not-a-package", 4, selectors ) == E_INVALIDARG );
    CHECK( "chunks-nonhex-id", ( package )->XPackageChangeChunkInstallOrder( "z0000001111122223333444444444444", 4,
                                                                             selectors ) == E_INVALIDARG );
    CHECK( "chunks-unknown-id",
           ( package )->XPackageChangeChunkInstallOrder( "00000099111122223333444444444444", 4, selectors ) ==
               E_GAMEPACKAGE_NO_PACKAGE_IDENTIFIER );
    CHECK( "chunks-unrelated-id",
           ( package )->XPackageChangeChunkInstallOrder( "00000006111122223333444444444444", 4, selectors ) ==
               E_GAMEPACKAGE_NO_PACKAGE_IDENTIFIER );
    bad.type = ( XPackageChunkSelectorType )99;
    CHECK( "chunks-invalid-kind", ( package )->XPackageChangeChunkInstallOrder( id, 1, &bad ) == E_INVALIDARG );
    bad.type = XPackageChunkSelectorType::Tag;
    CHECK( "chunks-null-tag", ( package )->XPackageChangeChunkInstallOrder( id, 1, &bad ) == E_INVALIDARG );
    bad.tag = "";
    CHECK( "chunks-empty-tag", ( package )->XPackageChangeChunkInstallOrder( id, 1, &bad ) == E_INVALIDARG );
    bad.type = XPackageChunkSelectorType::Feature;
    bad.feature = NULL;
    CHECK( "chunks-null-feature", ( package )->XPackageChangeChunkInstallOrder( id, 1, &bad ) == E_INVALIDARG );
    bad.type = XPackageChunkSelectorType::Language;
    CHECK( "chunks-null-language", ( package )->XPackageChangeChunkInstallOrder( id, 1, &bad ) == E_INVALIDARG );
}

static void current_child( const WCHAR *executable, BOOL missing, BOOL knownGame = TRUE )
{
    STARTUPINFOW startup = { 0 };
    PROCESS_INFORMATION process = { 0 };
    WCHAR command[MAX_PATH + 64];
    DWORD code = 99, wait;
    startup.cb = sizeof( startup );
    swprintf( command, ARRAY_SIZE( command ), L"\"%ls\" --chunks-%ls %ls", executable,
              missing ? L"missing" : L"current", knownGame ? L"--kind-game" : L"--kind-unknown" );
    CHECK( "chunks-child-start",
           CreateProcessW( executable, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process ) );
    if ( !process.hProcess ) return;
    wait = WaitForSingleObject( process.hProcess, 15000 );
    CHECK( "chunks-child-finished", wait == WAIT_OBJECT_0 );
    if ( wait != WAIT_OBJECT_0 ) TerminateProcess( process.hProcess, 77 );
    GetExitCodeProcess( process.hProcess, &code );
    CHECK( "chunks-child-pass", code == 0 );
    CloseHandle( process.hProcess );
    CloseHandle( process.hThread );
}

static void current_fixtures( void )
{
    WCHAR root[MAX_PATH], nested[MAX_PATH], executable[MAX_PATH], original[MAX_PATH], file[MAX_PATH];
    fixture( L"current-base", "Nexus.Base", "B00000000001", NULL, NULL, 9, TRUE );
    content_join( folder, L"current-base", root );
    content_join( root, L"Binaries", nested );
    CreateDirectoryW( nested, NULL );
    content_join( nested, L"chunks.exe", executable );
    CHECK( "chunks-self-path", GetModuleFileNameW( NULL, original, MAX_PATH ) );
    CHECK( "chunks-self-copy", CopyFileW( original, executable, TRUE ) );
    current_child( executable, FALSE );
    content_join( root, L"logo.png", file );
    DeleteFileW( file );
    current_child( executable, TRUE );
    write_text( file, "payload" );
    content_join( root, L"layout_00000009-1111-2222-3333-444444444444.xml", file );
    write_text( file, "<invalid/>" );
    current_child( executable, TRUE );
    fixture( L"current-base", "Nexus.Base", "B00000000001", NULL, NULL, 9, TRUE );
    content_join( root, L"appxmanifest.xml", file );
    write_text( file, "<Package><Identity Name=\"Wrong\"/></Package>" );
    current_child( executable, TRUE, FALSE );
}

static void actual_runtime( const char *child, BOOL knownGame = TRUE )
{
    struct options
    {
        UINT32 tag;
        BOOL is_inline;
        const char *config;
    } options = { 1, TRUE, main_config };
    HMODULE module = LoadLibraryW( L"xgameruntime.dll" );
    HRESULT( WINAPI * initialize )( ULONG, ULONG, char, const void * );
    HRESULT( WINAPI * query )( REFCLSID, REFIID, void ** );
    IXPackageImpl *package = NULL;
    IXStoreImpl *store = NULL;
    char identifier[35];
    CHECK( "runtime-load", module != NULL );
    if ( !module ) return;
    initialize = reinterpret_cast<decltype( initialize )>( GetProcAddress( module, "InitializeApiImplEx2" ) );
    query = reinterpret_cast<decltype( query )>( GetProcAddress( module, "QueryApiImpl" ) );
    CHECK( "runtime-exports", initialize && query );
    if ( !initialize || !query ) return;
    CHECK( "runtime-initialize", initialize( 0, 0, 0, &options ) == S_OK );
    CHECK( "runtime-query", query( CLSID_XPackageImpl, IID_IXPackageImpl, ( void ** )&package ) == S_OK && package );
    if ( !package ) return;
    if ( child )
    {
        IXPackageImpl4 *latest = nullptr;
        CHECK( "runtime-current-kind-interface",
               package->QueryInterface( __uuidof( IXPackageImpl4 ), reinterpret_cast<void **>( &latest ) ) == S_OK &&
                   latest );
        if ( latest )
        {
            XPackageKind kind = XPackageKind::PublisherContent;
            // Package type is independent of chunk completeness. A real base
            // manifest remains Game when payload is missing; a foreign manifest
            // must not be classified as the current game.
            const auto kindResult = latest->XPackageGetKind( "00000009111122223333444444444444", &kind );
            printf( "KIND result=%08lx knownGame=%u kind=%u\n", kindResult, knownGame, static_cast<UINT32>( kind ) );
            CHECK( "runtime-current-kind-actual",
                   kindResult == ( knownGame ? S_OK : E_GAMEPACKAGE_NO_PACKAGE_IDENTIFIER ) &&
                       ( !knownGame || kind == XPackageKind::Game ) );
            latest->Release();
        }

        chunk_order( package, "00000009111122223333444444444444",
                     !strcmp( child, "--chunks-current" ) ? S_OK : E_GAMEPACKAGE_NO_PACKAGE_IDENTIFIER );
        ( package )->Release();
        return;
    }
    chunk_order( package, "00000001111122223333444444444444", S_OK );
    {
        WCHAR root[MAX_PATH], file[MAX_PATH];
        XPackageChunkSelector selector{};
        selector.type = XPackageChunkSelectorType::Chunk;
        CHECK( "chunks-related-installed", ( package )->XPackageChangeChunkInstallOrder(
                                               "00000004111122223333444444444444", 1, &selector ) == S_OK );
        selector.chunkId = 0xffffffff;
        CHECK( "chunks-unscheduled-ignored", ( package )->XPackageChangeChunkInstallOrder(
                                                 "00000001111122223333444444444444", 1, &selector ) == S_OK );
        content_join( folder, L"own-one", root );
        content_join( root, L"logo.png", file );
        DeleteFileW( file );
        CHECK( "chunks-content-missing-file",
               ( package )->XPackageChangeChunkInstallOrder( "00000001111122223333444444444444", 1, &selector ) ==
                   E_GAMEPACKAGE_NO_PACKAGE_IDENTIFIER );
        write_text( file, "payload" );
    }
    IXPackageImpl4 *latest = nullptr;
    CHECK( "runtime-content-kind-interface",
           package->QueryInterface( __uuidof( IXPackageImpl4 ), reinterpret_cast<void **>( &latest ) ) == S_OK &&
               latest );
    if ( latest )
    {
        XPackageKind kind = XPackageKind::Game;
        CHECK( "runtime-content-kind-actual",
               latest->XPackageGetKind( "00000001111122223333444444444444", &kind ) == S_OK &&
                   kind == XPackageKind::Content );
        latest->Release();
    }
    expected = 2;
    visits = 0;
    stop_early = FALSE;
    CHECK( "runtime-enumerate", ( package )->XPackageEnumeratePackages(
                                    XPackageKind::Content, XPackageEnumerationScope::ThisOnly, NULL, visit ) == S_OK );
    CHECK( "runtime-content-count", visits == 2 );
    CHECK( "runtime-null-callback",
           ( package )->XPackageEnumeratePackages( XPackageKind::Content, XPackageEnumerationScope::ThisOnly, NULL,
                                                   NULL ) == E_INVALIDARG );
    CHECK( "runtime-invalid-kind",
           ( package )->XPackageEnumeratePackages( ( XPackageKind )99, XPackageEnumerationScope::ThisOnly, NULL,
                                                   visit ) == E_INVALIDARG );
    CHECK( "runtime-invalid-scope",
           ( package )->XPackageEnumeratePackages( XPackageKind::Content, ( XPackageEnumerationScope )99, NULL,
                                                   visit ) == E_INVALIDARG );
    ( package )->Release();
    CHECK( "runtime-store-query", query( CLSID_XStoreImpl, IID_IXStoreImpl, ( void ** )&store ) == S_OK && store );
    if ( store )
    {
        memset( identifier, '!', sizeof( identifier ) );
        CHECK( "runtime-store-package-id",
               ( store )->XStoreQueryPackageIdentifier( "D00000000001", 33, identifier + 1 ) == S_OK );
        CHECK( "runtime-store-package-value", !strcmp( identifier + 1, "00000001111122223333444444444444" ) );
        CHECK( "runtime-store-buffer-guards", identifier[0] == '!' && identifier[34] == '!' );
        CHECK( "runtime-store-package-unknown",
               ( store )->XStoreQueryPackageIdentifier( "D00000000099", 33, identifier + 1 ) == E_INVALIDARG &&
                   !identifier[1] );
        CHECK( "runtime-store-package-short", ( store )->XStoreQueryPackageIdentifier(
                                                  "D00000000001", 32, identifier + 1 ) == E_NOT_SUFFICIENT_BUFFER );
        CHECK( "runtime-store-package-null",
               ( store )->XStoreQueryPackageIdentifier( NULL, 33, identifier + 1 ) == E_INVALIDARG );
        CHECK( "runtime-store-current-not-installed",
               ( store )->XStoreQueryPackageIdentifier( "B00000000001", 33, identifier + 1 ) == E_INVALIDARG );
        ( store )->Release();
    }
}

int main( int argc, char **argv )
{
    WCHAR temporary[MAX_PATH];
    if ( argc >= 2 && ( !strcmp( argv[1], "--chunks-current" ) || !strcmp( argv[1], "--chunks-missing" ) ) )
    {
        actual_runtime( argv[1], argc < 3 || strcmp( argv[2], "--kind-unknown" ) );
        printf( "CHUNK CHILD CHECKS %u FAILURES %u\n", checks, failures );
        return failures ? 1 : 0;
    }
    GetTempPathW( MAX_PATH, temporary );
    swprintf( folder, MAX_PATH, L"%lsnexus-content-%lu-%lu", temporary, GetCurrentProcessId(), GetTickCount() );
    CHECK( "fixture-directory", CreateDirectoryW( folder, NULL ) );
    helpers();
    actual_runtime( NULL );
    current_fixtures();
    printf( "CHECKS %u FAILURES %u\n", checks, failures );
    return failures ? 1 : 0;
}
