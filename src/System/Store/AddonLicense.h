/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Package-free add-on licenses from fresh signed receipts. Catalog metadata and
 * installed content never grant rights. Included after xstore_package_license.h. */
#define ADDON_MAX_LICENSES ( 256 * ENTITLED_MAX_SKUS )
#define ADDON_RESULT_MAGIC 0x41444c31
struct addon_result_header
{
    UINT32 magic, count;
};
struct addon_license_async
{
    LONG refs, started, canceled;
    XStoreContextHandle store;
    UINT64 serial, until;
    UINT32 count;
    XStoreAddonLicense *licenses;
    char request[16384];
    IXThreadingImpl *threading;
    XAsyncBlock *async;
    struct addon_license_async *next;
};
static const char addon_license_identity{};
static SRWLOCK addon_license_lock = SRWLOCK_INIT;
static struct addon_license_async *addon_license_requests;

static void addon_license_release( struct addon_license_async *context )
{
    if ( InterlockedDecrement( &context->refs ) ) return;
    free( context->licenses );
    ( context->threading )->Release();
    free( context );
}

static void addon_license_retire( struct addon_license_async *context )
{
    struct addon_license_async **link;
    AcquireSRWLockExclusive( &addon_license_lock );
    for ( link = &addon_license_requests; *link && *link != context; link = &( *link )->next )
        ;
    if ( *link ) *link = context->next;
    ReleaseSRWLockExclusive( &addon_license_lock );
}

static struct addon_license_async *addon_license_find( XAsyncBlock *async )
{
    struct addon_license_async *context;
    AcquireSRWLockShared( &addon_license_lock );
    for ( context = addon_license_requests; context && context->async != async; context = context->next )
        ;
    if ( context ) InterlockedIncrement( &context->refs );
    ReleaseSRWLockShared( &addon_license_lock );
    return context;
}

static BOOL addon_license_context_valid( struct addon_license_async *context )
{
    UINT64 xuid, serial;
    return SUCCEEDED( entitled_store_identity( context->store, &xuid, &serial ) ) && serial == context->serial;
}

static BOOL addon_license_active_sku( const XStoreSku *sku, UINT64 now )
{
    return sku->isInUserCollection && !sku->isTrial && sku->collectionData.endDate > 0 &&
           ( UINT64 )sku->collectionData.endDate > now;
}

static HRESULT addon_licenses_from_snapshot( struct entitled_snapshot *snapshot, XStoreAddonLicense **out,
                                             UINT32 *count, UINT64 *until )
{
    XStoreAddonLicense *licenses;
    UINT32 i, j, total = 0;
    UINT64 now = time( NULL ), expiry, game_until = 0;
    BOOL digital_game = FALSE;
    *out = NULL;
    *count = 0;
    *until = 0;
    if ( !snapshot || snapshot->proof_until <= now || snapshot->count > ENTITLED_MAX_PRODUCTS ) return E_ACCESSDENIED;
    /* This API applies to a digital current-game license, not merely an owned
     * add-on. A missing digital game receipt produces an empty result. */
    for ( i = 0; i < snapshot->count; i++ )
    {
        XStoreProduct *product = &snapshot->products[i];
        if ( product->skusCount > ENTITLED_MAX_SKUS ) return E_UNEXPECTED;
        if ( strcmp( product->storeId, GetStoreProductId() ) || product->productKind != XStoreProductKind::Game ||
             !product->isInUserCollection || !product->hasDigitalDownload )
            continue;
        for ( j = 0; j < product->skusCount; j++ )
            if ( addon_license_active_sku( &product->skus[j], now ) )
            {
                digital_game = TRUE;
                game_until = std::max( game_until, ( UINT64 )product->skus[j].collectionData.endDate );
            }
    }
    *until = snapshot->proof_until;
    if ( !digital_game ) return S_OK;
    *until = std::min( *until, game_until );
    for ( i = 0; i < snapshot->count; i++ )
    {
        XStoreProduct *product = &snapshot->products[i];
        if ( product->productKind != XStoreProductKind::Durable || !product->isInUserCollection ||
             product->hasDigitalDownload )
            continue;
        if ( !entitled_id( product->storeId ) || product->skusCount > ENTITLED_MAX_SKUS || !product->inAppOfferToken ||
             strlen( product->inAppOfferToken ) >= sizeof( licenses[0].inAppOfferToken ) )
            return E_UNEXPECTED;
        for ( j = 0; j < product->skusCount; j++ )
            if ( addon_license_active_sku( &product->skus[j], now ) )
            {
                if ( !entitled_sku_id( product->skus[j].skuId ) || total == ADDON_MAX_LICENSES ) return E_UNEXPECTED;
                total++;
            }
    }
    if ( !total ) return S_OK;
    if ( !( licenses =
                static_cast<std::remove_reference_t<decltype( licenses )>>( calloc( total, sizeof( *licenses ) ) ) ) )
        return E_OUTOFMEMORY;
    total = 0;
    for ( i = 0; i < snapshot->count; i++ )
    {
        XStoreProduct *product = &snapshot->products[i];
        if ( product->productKind != XStoreProductKind::Durable || !product->isInUserCollection ||
             product->hasDigitalDownload )
            continue;
        for ( j = 0; j < product->skusCount; j++ )
        {
            XStoreSku *sku = &product->skus[j];
            if ( !addon_license_active_sku( sku, now ) ) continue;
            expiry = std::min( std::min( snapshot->proof_until, game_until ), ( UINT64 )sku->collectionData.endDate );
            snprintf( licenses[total].skuStoreId, sizeof( licenses[total].skuStoreId ), "%s/%s", product->storeId,
                      sku->skuId );
            strcpy( licenses[total].inAppOfferToken, product->inAppOfferToken );
            licenses[total].isActive = TRUE;
            licenses[total++].expirationDate = expiry;
            *until = std::min( *until, expiry );
        }
    }
    *out = licenses;
    *count = total;
    return S_OK;
}

static DWORD WINAPI addon_license_worker( void *arg )
{
    addon_license_async *context = static_cast<addon_license_async *>( arg );
    struct entitled_snapshot *snapshot = NULL;
    HRESULT hr = E_ABORT;
    if ( !InterlockedCompareExchange( &context->canceled, 0, 0 ) )
        hr = entitled_fetch( context->request, &context->canceled, &snapshot );
    if ( SUCCEEDED( hr ) && !addon_license_context_valid( context ) ) hr = E_INVALIDARG;
    if ( SUCCEEDED( hr ) )
        hr = addon_licenses_from_snapshot( snapshot, &context->licenses, &context->count, &context->until );
    if ( snapshot ) entitled_snapshot_release( snapshot );
    if ( InterlockedCompareExchange( &context->canceled, 0, 0 ) ) hr = E_ABORT;
    xodus_store_license_note( "store-addon-license-result", hr );
    ( context->threading )
        ->XAsyncComplete( context->async, hr,
                          SUCCEEDED( hr )
                              ? sizeof( struct addon_result_header ) + context->count * sizeof( XStoreAddonLicense )
                              : 0 );
    addon_license_release( context );
    return 0;
}

static HRESULT WINAPI addon_license_provider( XAsyncOp op, const XAsyncProviderData *data )
{
    addon_license_async *context = static_cast<addon_license_async *>( data->context );
    struct addon_result_header header;
    HANDLE worker;
    HRESULT hr;
    switch ( op )
    {
    case XAsyncOp::Begin:
        return ( context->threading )->XAsyncSchedule( data->async, 0 );
    case XAsyncOp::DoWork:
        if ( InterlockedCompareExchange( &context->started, 1, 0 ) ) return E_PENDING;
        if ( InterlockedCompareExchange( &context->canceled, 0, 0 ) ) return E_ABORT;
        InterlockedIncrement( &context->refs );
        if ( !( worker = CreateThread( NULL, 0, addon_license_worker, context, 0, NULL ) ) )
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );
            addon_license_release( context );
            return hr;
        }
        CloseHandle( worker );
        return E_PENDING;
    case XAsyncOp::GetResult:
        if ( !data->buffer || data->bufferSize < sizeof( header ) + context->count * sizeof( XStoreAddonLicense ) )
            return E_INVALIDARG;
        if ( !addon_license_context_valid( context ) ) return E_INVALIDARG;
        if ( context->until <= ( UINT64 )time( NULL ) ) return E_ACCESSDENIED;
        header.magic = ADDON_RESULT_MAGIC;
        header.count = context->count;
        memcpy( data->buffer, &header, sizeof( header ) );
        if ( context->count )
            memcpy( ( char * )data->buffer + sizeof( header ), context->licenses,
                    context->count * sizeof( XStoreAddonLicense ) );
        break;
    case XAsyncOp::Cancel:
        InterlockedExchange( &context->canceled, TRUE );
        break;
    case XAsyncOp::Cleanup:
        addon_license_retire( context );
        addon_license_release( context );
        break;
    default:
        break;
    }
    return S_OK;
}

static HRESULT addon_license_begin( XStoreContextHandle store, XAsyncBlock *async )
{
    struct addon_license_async *context;
    UINT64 xuid, serial;
    UINT32 i;
    SIZE_T size;
    int written;
    HRESULT hr;
    if ( !async ) return E_INVALIDARG;
    if ( FAILED( hr = entitled_store_identity( store, &xuid, &serial ) ) ) return hr;
    if ( !entitled_id( GetStoreProductId() ) ) return E_GAME_MISSING_GAME_CONFIG;
    if ( !( context = static_cast<std::remove_reference_t<decltype( context )>>( calloc( 1, sizeof( *context ) ) ) ) )
        return E_OUTOFMEMORY;
    context->refs = 1;
    context->store = store;
    context->serial = serial;
    context->async = async;
    size = snprintf( context->request, sizeof( context->request ), "<Request><ParentProductId>%s</ParentProductId>",
                     GetStoreProductId() );
    for ( i = 0; i < entitled_related_count; i++ )
    {
        if ( !strcmp( GetStoreProductId(), entitled_related[i] ) ) continue;
        written = snprintf( context->request + size, sizeof( context->request ) - size, "<ProductId>%s</ProductId>",
                            entitled_related[i] );
        if ( written < 0 || ( SIZE_T )written >= sizeof( context->request ) - size )
        {
            free( context );
            return E_INVALIDARG;
        }
        size += written;
    }
    written = snprintf( context->request + size, sizeof( context->request ) - size,
                        "<ProductKinds>6</ProductKinds><Market>US</Market><Language>en-US</Language></Request>" );
    if ( written < 0 || ( SIZE_T )written >= sizeof( context->request ) - size )
    {
        free( context );
        return E_INVALIDARG;
    }
    if ( FAILED( hr = QueryApiImpl( CLSID_XThreadingImpl, IID_IXThreadingImpl, ( void ** )&context->threading ) ) )
    {
        free( context );
        return hr;
    }
    AcquireSRWLockExclusive( &addon_license_lock );
    context->next = addon_license_requests;
    addon_license_requests = context;
    ReleaseSRWLockExclusive( &addon_license_lock );
    hr = ( context->threading )
             ->XAsyncBegin( async, context, &addon_license_identity, "XStoreQueryAddOnLicensesAsync",
                            addon_license_provider );
    if ( FAILED( hr ) )
    {
        addon_license_retire( context );
        addon_license_release( context );
    }
    return hr;
}

static HRESULT addon_license_count( XAsyncBlock *async, UINT32 *count )
{
    struct addon_license_async *context;
    IXThreadingImpl *threading;
    SIZE_T size;
    HRESULT hr;
    if ( !async || !count ) return E_INVALIDARG;
    *count = 0;
    if ( FAILED( hr = QueryApiImpl( CLSID_XThreadingImpl, IID_IXThreadingImpl, ( void ** )&threading ) ) ) return hr;
    /* Querying the size never consumes the result. The private typed registry
     * rejects a successful async operation belonging to a different API. */
    context = addon_license_find( async );
    hr = ( threading )->XAsyncGetResultSize( async, &size );
    if ( SUCCEEDED( hr ) )
    {
        if ( !context )
            hr = E_INVALIDARG;
        else if ( size != sizeof( struct addon_result_header ) + context->count * sizeof( XStoreAddonLicense ) )
            hr = E_UNEXPECTED;
        else
            *count = context->count;
    }
    if ( context ) addon_license_release( context );
    ( threading )->Release();
    return hr;
}

static HRESULT addon_license_result( XAsyncBlock *async, UINT32 count, XStoreAddonLicense *out )
{
    struct addon_result_header *buffer;
    IXThreadingImpl *threading;
    UINT32 needed;
    SIZE_T size;
    HRESULT hr;
    if ( FAILED( hr = addon_license_count( async, &needed ) ) ) return hr;
    if ( count < needed ) return E_NOT_SUFFICIENT_BUFFER;
    if ( needed && !out ) return E_INVALIDARG;
    size = sizeof( *buffer ) + needed * sizeof( *out );
    if ( !( buffer = static_cast<std::remove_reference_t<decltype( buffer )>>( malloc( size ) ) ) )
        return E_OUTOFMEMORY;
    if ( SUCCEEDED( hr = QueryApiImpl( CLSID_XThreadingImpl, IID_IXThreadingImpl, ( void ** )&threading ) ) )
    {
        hr = ( threading )->XAsyncGetResult( async, &addon_license_identity, size, buffer, NULL );
        if ( SUCCEEDED( hr ) )
        {
            if ( buffer->magic != ADDON_RESULT_MAGIC || buffer->count != needed )
                hr = E_UNEXPECTED;
            else if ( needed )
                memcpy( out, buffer + 1, needed * sizeof( *out ) );
        }
        ( threading )->Release();
    }
    free( buffer );
    return hr;
}
