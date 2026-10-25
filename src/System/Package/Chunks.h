/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Read-only snapshots of extracted package chunk metadata. No installation,
 * licensing or download capability is inferred from a layout declaration. */
#define CHUNKS_MAX 4096
#define CHUNKS_TEXT_MAX 1024
#define CHUNKS_FILES_MAX 65536
#include <cstdio>

struct chunk_group
{
    WCHAR pattern[MAX_PATH];
    UINT chunk;
};

struct chunk_file
{
    WCHAR path[MAX_PATH];
    UINT64 bytes;
    UINT chunk; /* CHUNKS_MAX denotes the registration chunk. */
};

struct installed_chunk
{
    UINT32 id;
    char *attributes[3]; /* language, tag, feature */
    XPackageChunkAvailability availability;
};
struct chunk_snapshot
{
    UINT count, groups;
    XPackageChunkAvailability registration;
    BOOL track_sizes;
    UINT file_count, file_capacity;
    struct chunk_file *files;
    UINT group_count, group_capacity;
    struct chunk_group *file_groups;
    struct installed_chunk chunks[CHUNKS_MAX];
};

static void chunks_free( struct chunk_snapshot *snapshot )
{
    UINT i, j;
    if ( !snapshot ) return;
    for ( i = 0; i < snapshot->count; i++ )
        for ( j = 0; j < 3; j++ )
            free( snapshot->chunks[i].attributes[j] );
    free( snapshot->files );
    free( snapshot->file_groups );
    free( snapshot );
}

static BOOL chunks_identifier( const char *id )
{
    UINT i;
    if ( !id || strlen( id ) != 32 ) return FALSE;
    for ( i = 0; i < 32; i++ )
        if ( !( ( id[i] >= '0' && id[i] <= '9' ) || ( id[i] >= 'a' && id[i] <= 'f' ) ||
                ( id[i] >= 'A' && id[i] <= 'F' ) ) )
            return FALSE;
    return TRUE;
}

/* Resolve only the running package or content already admitted by the shared
 * related-content discovery rules. Never use an identifier as a filesystem path. */
static HRESULT chunks_resolve( const char *id, WCHAR *out )
{
    struct content_package *package;
    struct content_snapshot *snapshot;
    struct content_game current;
    WCHAR path[MAX_PATH], file[MAX_PATH], pattern[MAX_PATH], *sep;
    WIN32_FIND_DATAW data;
    HANDLE find;
    char candidate[33];
    UINT i, matches = 0;
    HRESULT hr = E_GAMEPACKAGE_NO_PACKAGE_IDENTIFIER;

    if ( !chunks_identifier( id ) ) return E_INVALIDARG;
    if ( !( package = static_cast<std::remove_reference_t<decltype( package )>>( calloc( 1, sizeof( *package ) ) ) ) )
        return E_OUTOFMEMORY;
    current = GetContentConfiguration();
    if ( GetModuleFileNameW( NULL, path, MAX_PATH ) && ( sep = wcsrchr( path, '\\' ) ) )
    {
        *sep = 0;
        while ( content_join( path, L"MicrosoftGame.config", file ) )
        {
            if ( GetFileAttributesW( file ) != INVALID_FILE_ATTRIBUTES )
            {
                if ( content_load( path, package ) && !package->content &&
                     ( !current.identity[0] || ( !strcmp( current.identity, package->identity ) &&
                                                 !strcmp( current.publisher, package->publisher_id ) &&
                                                 !strcmp( current.store, package->store ) ) ) &&
                     content_join( path, L"layout_*.xml", pattern ) &&
                     ( find = FindFirstFileW( pattern, &data ) ) != INVALID_HANDLE_VALUE )
                {
                    do
                    {
                        if ( !( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) &&
                             content_layout_id( data.cFileName, candidate ) && !stricmp( candidate, id ) )
                            matches++;
                    } while ( FindNextFileW( find, &data ) );
                    FindClose( find );
                    if ( matches == 1 )
                    {
                        wcscpy( out, path );
                        free( package );
                        return S_OK;
                    }
                }
                break;
            }
            if ( !( sep = wcsrchr( path, '\\' ) ) || sep <= path + 2 ) break;
            *sep = 0;
        }
    }
    free( package );
    if ( !( snapshot =
                static_cast<std::remove_reference_t<decltype( snapshot )>>( calloc( 1, sizeof( *snapshot ) ) ) ) )
        return E_OUTOFMEMORY;
    snapshot->current = current;
    if ( SUCCEEDED( hr = content_discover( snapshot ) ) )
    {
        hr = E_GAMEPACKAGE_NO_PACKAGE_IDENTIFIER;
        matches = 0;
        for ( i = 0; i < snapshot->count; i++ )
            if ( !stricmp( snapshot->packages[i].identifier, id ) &&
                 content_matches( snapshot, &snapshot->packages[i], XPackageEnumerationScope::ThisAndRelated ) )
            {
                wcscpy( out, snapshot->packages[i].path );
                matches++;
            }
        if ( matches == 1 ) hr = S_OK;
    }
    free( snapshot );
    return hr;
}

static BOOL chunks_number( const char *text, UINT32 *number )
{
    UINT64 value = 0;
    if ( !text || !*text ) return FALSE;
    for ( ; *text; text++ )
    {
        if ( *text < '0' || *text > '9' ) return FALSE;
        value = value * 10 + *text - '0';
        if ( value > 0xffffffffu ) return FALSE;
    }
    *number = value;
    return TRUE;
}

/* Token iteration owns no storage and never mutates the XML/caller's buffer. */
static BOOL chunks_token( const char **cursor, char *out )
{
    const char *start = *cursor, *end;
    SIZE_T length;
    if ( !start || !*start ) return FALSE;
    end = strchr( start, ';' );
    if ( !end ) end = start + strlen( start );
    *cursor = *end ? end + 1 : NULL;
    while ( start < end && ( *start == ' ' || *start == '\t' ) )
        start++;
    while ( end > start && ( end[-1] == ' ' || end[-1] == '\t' ) )
        end--;
    length = end - start;
    if ( !length || length > CHUNKS_TEXT_MAX )
    {
        out[0] = 0;
        return TRUE;
    }
    memcpy( out, start, length );
    out[length] = 0;
    return TRUE;
}

static BOOL chunks_contains( const char *list, const char *value, BOOL language )
{
    char token[CHUNKS_TEXT_MAX + 1];
    while ( chunks_token( &list, token ) )
        if ( language ? !stricmp( token, value ) : !strcmp( token, value ) ) return TRUE;
    return FALSE;
}

static HRESULT chunks_attribute( xmlNodePtr node, const char *name, char **out )
{
    xmlChar *value = xmlGetProp( node, ( xmlChar * )name );
    const char *cursor;
    char token[CHUNKS_TEXT_MAX + 1];
    HRESULT hr = S_OK;
    *out = NULL;
    if ( !value ) return S_OK;
    cursor = ( char * )value;
    if ( !*cursor || strlen( cursor ) > CHUNKS_TEXT_MAX || cursor[strlen( cursor ) - 1] == ';' )
        hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
    while ( SUCCEEDED( hr ) && chunks_token( &cursor, token ) )
        if ( !token[0] ) hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
    if ( SUCCEEDED( hr ) && !( *out = strdup( ( char * )value ) ) ) hr = E_OUTOFMEMORY;
    xmlFree( value );
    return hr;
}

/* SourcePath is a build-machine path when DestinationPath exists. Only the
 * extracted destination is queried, with Windows case-insensitive lookup.
 * Symbolic links in a legitimate temporary launch view are readable files too;
 * no file content is read, and no ownership assertion is made. */
static HRESULT chunks_record_file( struct chunk_snapshot *snapshot, UINT chunk, const WCHAR *path, UINT64 bytes )
{
    struct chunk_file *files, *file;
    UINT capacity;
    if ( !snapshot->track_sizes ) return S_OK;
    if ( snapshot->file_count == CHUNKS_FILES_MAX ) return HRESULT_FROM_WIN32( ERROR_MORE_DATA );
    if ( snapshot->file_count == snapshot->file_capacity )
    {
        capacity = snapshot->file_capacity ? snapshot->file_capacity * 2 : 64;
        if ( !( files = static_cast<std::remove_reference_t<decltype( files )>>(
                    realloc( snapshot->files, capacity * sizeof( *files ) ) ) ) )
            return E_OUTOFMEMORY;
        snapshot->files = files;
        snapshot->file_capacity = capacity;
    }
    file = &snapshot->files[snapshot->file_count++];
    wcscpy( file->path, path );
    file->bytes = bytes;
    file->chunk = chunk;
    return S_OK;
}

static HRESULT chunks_files( const WCHAR *root, xmlNodePtr chunk, struct chunk_snapshot *snapshot, UINT chunk_index,
                             XPackageChunkAvailability *availability, BOOL inspect_files )
{
    xmlNodePtr node;
    char location[MAX_PATH], include[MAX_PATH];
    const char *relative;
    WCHAR wide[MAX_PATH], directory[MAX_PATH], pattern[MAX_PATH], file[MAX_PATH];
    WCHAR *p;
    struct chunk_group *groups, *group;
    UINT capacity;
    *availability = inspect_files ? XPackageChunkAvailability::Ready : XPackageChunkAvailability::Unavailable;
    for ( node = chunk->children; node; node = node->next )
    {
        if ( node->type != XML_ELEMENT_NODE ) continue;
        if ( strcmp( ( char * )node->name, "FileGroup" ) || ++snapshot->groups > 16384 ||
             !content_text( node, "Include", include, sizeof( include ) ) || !content_relative( include, TRUE ) )
            return HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
        if ( xmlHasProp( node, ( xmlChar * )"Exclude" ) || node->children ) return E_NOTIMPL;
        relative = location;
        if ( xmlHasProp( node, ( xmlChar * )"DestinationPath" ) )
        {
            if ( !content_text( node, "DestinationPath", location, sizeof( location ) ) )
                return HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
            if ( *relative == '\\' || *relative == '/' ) relative++;
            if ( !*relative ) relative = ".";
        }
        else if ( !content_text( node, "SourcePath", location, sizeof( location ) ) )
            return HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
        if ( !content_relative( relative, FALSE ) ||
             !MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, relative, -1, wide, MAX_PATH ) )
            return HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
        for ( p = wide; *p; p++ )
            if ( *p == '/' ) *p = '\\';
        if ( !content_join( root, wide, directory ) ||
             !MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, include, -1, wide, MAX_PATH ) )
            return HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
        for ( p = wide; *p; p++ )
            if ( *p == '/' ) *p = '\\';
        if ( !content_join( directory, wide, pattern ) ) return HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
        /* Validate every declaration, even outside the requested selection.
         * Only selected chunks may contribute to a monitor's availability or
         * byte total, so unrelated file opens cannot improve that snapshot. */
        if ( !inspect_files ) continue;
        if ( snapshot->group_count == snapshot->group_capacity )
        {
            capacity = snapshot->group_capacity ? snapshot->group_capacity * 2 : 64;
            if ( !( groups =
                        static_cast<chunk_group *>( realloc( snapshot->file_groups, capacity * sizeof( *groups ) ) ) ) )
                return E_OUTOFMEMORY;
            snapshot->file_groups = groups;
            snapshot->group_capacity = capacity;
        }
        group = &snapshot->file_groups[snapshot->group_count++];
        wcscpy( group->pattern, pattern );
        group->chunk = chunk_index;
    }
    return S_OK;
}

/* A hint changes inspection order, never availability. It is used only after
 * re-reading and validating the current layout, and only if its full pattern
 * is still part of this selection. Every use re-opens the actual filesystem.
 * One bounded process-local entry avoids a cache of package or account state.
 */
static SRWLOCK chunks_hint_lock = SRWLOCK_INIT;
static WCHAR chunks_hint[MAX_PATH];
static LONG chunks_warnings;

static void chunks_remember_missing( const WCHAR *pattern )
{
    BOOL changed;
    AcquireSRWLockExclusive( &chunks_hint_lock );
    changed = wcsicmp( chunks_hint, pattern ) != 0;
    wcscpy( chunks_hint, pattern );
    ReleaseSRWLockExclusive( &chunks_hint_lock );
    if ( changed && InterlockedIncrement( &chunks_warnings ) <= 8 )
        std::fprintf( stderr, "NEXUS_PACKAGE_DIAGNOSTIC_V1 missing-or-unreadable: "
                              "required package files are "
                              "missing or unreadable. Restore removed files or "
                              "repair the installation.\n" );
}

static HRESULT chunks_inspect_group( struct chunk_snapshot *snapshot, UINT index )
{
    const struct chunk_group *group = &snapshot->file_groups[index];
    XPackageChunkAvailability *availability =
        group->chunk == CHUNKS_MAX ? &snapshot->registration : &snapshot->chunks[group->chunk].availability;
    WIN32_FIND_DATAW data;
    HANDLE find, readable;
    LARGE_INTEGER size;
    WCHAR file[MAX_PATH], *leaf;
    UINT entries = 0;
    BOOL found = FALSE;
    HRESULT hr = S_OK;
    if ( ( find = FindFirstFileW( group->pattern, &data ) ) == INVALID_HANDLE_VALUE )
    {
        *availability = XPackageChunkAvailability::Unavailable;
        return S_OK;
    }
    do
    {
        if ( ++entries > 16384 )
        {
            hr = HRESULT_FROM_WIN32( ERROR_MORE_DATA );
            break;
        }
        if ( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) continue;
        found = TRUE;
        /* Include can contain a relative subdirectory before its leaf. */
        wcscpy( file, group->pattern );
        if ( ( leaf = wcsrchr( file, '\\' ) ) )
            leaf[1] = 0;
        else
        {
            hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
            break;
        }
        if ( wcslen( file ) + wcslen( data.cFileName ) >= MAX_PATH )
        {
            hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
            break;
        }
        wcscat( file, data.cFileName );
        readable = CreateFileW( file, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
        if ( readable == INVALID_HANDLE_VALUE )
            *availability = XPackageChunkAvailability::Unavailable;
        else
        {
            hr = S_OK;
            if ( snapshot->track_sizes )
            {
                if ( !GetFileSizeEx( readable, &size ) || size.QuadPart < 0 )
                    *availability = XPackageChunkAvailability::Unavailable;
                else
                    hr = chunks_record_file( snapshot, group->chunk, file, size.QuadPart );
            }
            CloseHandle( readable );
        }
        if ( FAILED( hr ) || ( snapshot->track_sizes && *availability != XPackageChunkAvailability::Ready ) ) break;
    } while ( FindNextFileW( find, &data ) );
    FindClose( find );
    if ( !found ) *availability = XPackageChunkAvailability::Unavailable;
    return hr;
}

static HRESULT chunks_inspect_files( struct chunk_snapshot *snapshot )
{
    WCHAR hint[MAX_PATH];
    UINT i, first = snapshot->group_count;
    HRESULT hr;
    /* Availability enumeration still inspects each requested chunk separately.
     * A monitor reports unknown totals as soon as ANY required group is absent,
     * so checking the remaining groups cannot improve that incomplete result. */
    if ( snapshot->track_sizes )
    {
        AcquireSRWLockShared( &chunks_hint_lock );
        wcscpy( hint, chunks_hint );
        ReleaseSRWLockShared( &chunks_hint_lock );
        if ( hint[0] )
            for ( i = 0; i < snapshot->group_count; i++ )
                if ( !wcsicmp( hint, snapshot->file_groups[i].pattern ) )
                {
                    first = i;
                    break;
                }
    }
    for ( i = 0; i <= snapshot->group_count; i++ )
    {
        const UINT index = i ? i - 1 : first;
        if ( index == snapshot->group_count || ( i && index == first ) ) continue;
        const struct chunk_group *group = &snapshot->file_groups[index];
        if ( FAILED( hr = chunks_inspect_group( snapshot, index ) ) ) return hr;
        const auto available =
            group->chunk == CHUNKS_MAX ? snapshot->registration : snapshot->chunks[group->chunk].availability;
        if ( snapshot->track_sizes && available != XPackageChunkAvailability::Ready )
        {
            chunks_remember_missing( group->pattern );
            return S_OK;
        }
    }
    return S_OK;
}

static HRESULT chunks_features( struct chunk_snapshot *snapshot, xmlNodePtr root )
{
    xmlNodePtr feature, features = content_child( root, "Features" );
    UINT i;
    for ( feature = features ? features->children : NULL; feature; feature = feature->next )
    {
        char id[256], *tags = NULL;
        HRESULT hr;
        if ( feature->type != XML_ELEMENT_NODE ) continue;
        if ( strcmp( ( char * )feature->name, "Feature" ) || !content_text( feature, "Id", id, sizeof( id ) ) ||
             strchr( id, ';' ) )
            return HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
        if ( FAILED( hr = chunks_attribute( feature, "Tags", &tags ) ) ) return hr;
        if ( !tags ) continue;
        for ( i = 0; i < snapshot->count; i++ )
        {
            struct installed_chunk *chunk = &snapshot->chunks[i];
            const char *cursor = tags;
            char tag[CHUNKS_TEXT_MAX + 1];
            BOOL match = FALSE;
            while ( chunks_token( &cursor, tag ) )
                if ( chunks_contains( chunk->attributes[1], tag, FALSE ) ) match = TRUE;
            if ( match && !chunks_contains( chunk->attributes[2], id, FALSE ) )
            {
                char *value;
                SIZE_T length = chunk->attributes[2] ? strlen( chunk->attributes[2] ) : 0;
                if ( length + strlen( id ) + 1 > CHUNKS_TEXT_MAX )
                {
                    free( tags );
                    return HRESULT_FROM_WIN32( ERROR_MORE_DATA );
                }
                if ( !( value = static_cast<std::remove_reference_t<decltype( value )>>(
                            malloc( length + strlen( id ) + 2 ) ) ) )
                {
                    free( tags );
                    return E_OUTOFMEMORY;
                }
                if ( length )
                {
                    memcpy( value, chunk->attributes[2], length );
                    value[length++] = ';';
                }
                strcpy( value + length, id );
                free( chunk->attributes[2] );
                chunk->attributes[2] = value;
            }
        }
        free( tags );
    }
    return S_OK;
}

static BOOL chunks_selected( const struct installed_chunk *chunk, UINT32 count,
                              const XPackageChunkSelector *selectors )
{
    if ( !count ) return TRUE;
    for ( UINT32 i = 0; i < count; i++ )
    {
        const XPackageChunkSelector *selector = &selectors[i];
        if ( selector->type == XPackageChunkSelectorType::Chunk )
        {
            if ( chunk->id == selector->chunkId ) return TRUE;
        }
        else
        {
            int attribute = selector->type == XPackageChunkSelectorType::Language ? 0
                            : selector->type == XPackageChunkSelectorType::Tag    ? 1
                                                                                  : 2;
            if ( !chunk->attributes[attribute] ||
                 chunks_contains( chunk->attributes[attribute], selector->language, attribute == 0 ) )
                return TRUE;
        }
    }
    return FALSE;
}

static HRESULT chunks_load( const char *identifier, BOOL track_sizes, struct chunk_snapshot **out,
                            UINT32 selector_count = 0, const XPackageChunkSelector *selectors = NULL )
{
    WCHAR path[MAX_PATH], pattern[MAX_PATH], file[MAX_PATH];
    WIN32_FIND_DATAW data;
    HANDLE find;
    xmlDocPtr doc = NULL;
    xmlNodePtr root, node;
    xmlNodePtr registration_node = NULL, chunk_nodes[CHUNKS_MAX] = { 0 };
    struct chunk_snapshot *snapshot;
    char candidate[33], id[64];
    UINT layouts = 0, i;
    BOOL registration = FALSE, layout_valid = TRUE;
    HRESULT hr;
    *out = NULL;
    if ( selector_count > CHUNKS_MAX || ( selector_count && !selectors ) ) return E_INVALIDARG;
    if ( FAILED( hr = chunks_resolve( identifier, path ) ) ) return hr;
    if ( !( snapshot =
                static_cast<std::remove_reference_t<decltype( snapshot )>>( calloc( 1, sizeof( *snapshot ) ) ) ) )
        return E_OUTOFMEMORY;
    snapshot->track_sizes = track_sizes;
    if ( !content_join( path, L"layout_*.xml", pattern ) ||
         ( find = FindFirstFileW( pattern, &data ) ) == INVALID_HANDLE_VALUE )
    {
        chunks_free( snapshot );
        return E_GAMEPACKAGE_NO_PACKAGE_IDENTIFIER;
    }
    hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
    do
    {
        if ( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) continue;
        if ( ++layouts != 1 || !content_layout_id( data.cFileName, candidate ) || stricmp( candidate, identifier ) ||
             !content_join( path, data.cFileName, file ) )
        {
            layout_valid = FALSE;
            break;
        }
    } while ( FindNextFileW( find, &data ) );
    FindClose( find );
    if ( !layout_valid || layouts != 1 ) goto done;
    doc = content_read_xml( file );
    root = doc ? xmlDocGetRootElement( doc ) : NULL;
    if ( !root || strcmp( ( char * )root->name, "Package" ) ) goto done;
    for ( node = root->children; node; node = node->next )
    {
        struct installed_chunk *chunk;
        if ( node->type != XML_ELEMENT_NODE ) continue;
        if ( strcmp( ( char * )node->name, "Chunk" ) ) continue;
        if ( !content_text( node, "Id", id, sizeof( id ) ) ) goto done;
        if ( !strcmp( id, "Registration" ) )
        {
            if ( registration ) goto done;
            registration = TRUE;
            registration_node = node;
            continue;
        }
        if ( snapshot->count == CHUNKS_MAX )
        {
            hr = HRESULT_FROM_WIN32( ERROR_MORE_DATA );
            goto done;
        }
        chunk = &snapshot->chunks[snapshot->count++];
        chunk_nodes[snapshot->count - 1] = node;
        if ( !chunks_number( id, &chunk->id ) ) goto done;
        for ( i = 0; i + 1 < snapshot->count; i++ )
            if ( snapshot->chunks[i].id == chunk->id ) goto done;
        if ( FAILED( hr = chunks_attribute( node, "Languages", &chunk->attributes[0] ) ) ||
             FAILED( hr = chunks_attribute( node, "Tags", &chunk->attributes[1] ) ) ||
             FAILED( hr = chunks_attribute( node, "Features", &chunk->attributes[2] ) ) )
            goto done;
        hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
    }
    if ( FAILED( hr = chunks_features( snapshot, root ) ) ) goto done;
    /* Feature selectors can be derived from tags. Resolve all attributes
     * before deciding which files the caller actually needs to inspect.
     * There is no persistent snapshot: every refresh rechecks selected files. */
    if ( registration_node &&
         FAILED( hr = chunks_files( path, registration_node, snapshot, CHUNKS_MAX, &snapshot->registration, TRUE ) ) )
        goto done;
    for ( i = 0; i < snapshot->count; i++ )
        if ( FAILED( hr = chunks_files( path, chunk_nodes[i], snapshot, i, &snapshot->chunks[i].availability,
                                        chunks_selected( &snapshot->chunks[i], selector_count, selectors ) ) ) )
            goto done;
    if ( FAILED( hr = chunks_inspect_files( snapshot ) ) ) goto done;
    *out = snapshot;
    snapshot = NULL;
    hr = S_OK;
done:
    if ( doc ) xmlFreeDoc( doc );
    chunks_free( snapshot );
    return hr;
}

static HRESULT enumerate_installed_chunks( const char *identifier, XPackageChunkSelectorType type, void *context,
                                           XPackageChunkAvailabilityCallback *callback )
{
    struct chunk_snapshot *snapshot;
    XPackageChunkSelector selector{};
    UINT i, j;
    int attribute;
    HRESULT hr;
    if ( !callback ) return E_INVALIDARG;
    switch ( type )
    {
    case XPackageChunkSelectorType::Chunk:
        attribute = -1;
        break;
    case XPackageChunkSelectorType::Language:
        attribute = 0;
        break;
    case XPackageChunkSelectorType::Tag:
        attribute = 1;
        break;
    case XPackageChunkSelectorType::Feature:
        attribute = 2;
        break;
    default:
        return E_INVALIDARG;
    }
    if ( FAILED( hr = chunks_load( identifier, FALSE, &snapshot ) ) ) return hr;
    selector.type = type;
    /* Each call owns a stable snapshot, holds no lock while calling title code,
     * and can safely be reentered or stopped by the callback. */
    for ( i = 0; i < snapshot->count; i++ )
    {
        struct installed_chunk *chunk = &snapshot->chunks[i];
        const char *cursor;
        char value[CHUNKS_TEXT_MAX + 1];
        if ( attribute < 0 )
        {
            XPackageChunkAvailability availability = std::max( snapshot->registration, chunk->availability );
            selector.chunkId = chunk->id;
            if ( !callback( context, &selector, availability ) ) break;
            continue;
        }
        cursor = chunk->attributes[attribute];
        while ( chunks_token( &cursor, value ) )
        {
            XPackageChunkAvailability availability = snapshot->registration;
            BOOL duplicate = FALSE;
            for ( j = 0; j < i; j++ )
                if ( chunks_contains( snapshot->chunks[j].attributes[attribute], value, attribute == 0 ) )
                    duplicate = TRUE;
            /* Repeated values within this chunk are emitted only once too. */
            if ( !duplicate )
            {
                const char *earlier = chunk->attributes[attribute];
                char token[CHUNKS_TEXT_MAX + 1];
                while ( chunks_token( &earlier, token ) )
                {
                    if ( earlier == cursor ) break;
                    if ( attribute == 0 ? !stricmp( token, value ) : !strcmp( token, value ) )
                    {
                        duplicate = TRUE;
                        break;
                    }
                }
            }
            if ( duplicate ) continue;
            for ( j = 0; j < snapshot->count; j++ )
                if ( !snapshot->chunks[j].attributes[attribute] ||
                     chunks_contains( snapshot->chunks[j].attributes[attribute], value, attribute == 0 ) )
                    availability = std::max( availability, snapshot->chunks[j].availability );
            selector.language = value;
            if ( !callback( context, &selector, availability ) ) goto done;
        }
    }
done:
    chunks_free( snapshot );
    return S_OK;
}
