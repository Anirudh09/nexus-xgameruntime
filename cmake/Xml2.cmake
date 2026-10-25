# Compile the exact XML parser already carried by the pinned Wine source.
# No system parser, network fetch, or dependency-version fallback is permitted.
if(NOT NEXUS_WINE_SOURCE)
    message(FATAL_ERROR "Set NEXUS_WINE_SOURCE to the matching pinned Wine source tree")
endif()
set(XML2_SOURCE "${NEXUS_WINE_SOURCE}/libs/xml2")
file(READ "${CMAKE_CURRENT_LIST_DIR}/xml2.lock.json" XML2_LOCK)
string(JSON XML2_COUNT LENGTH "${XML2_LOCK}" files)
math(EXPR XML2_LAST "${XML2_COUNT} - 1")
foreach(INDEX RANGE ${XML2_LAST})
    string(JSON NAME MEMBER "${XML2_LOCK}" files ${INDEX})
    string(JSON EXPECTED GET "${XML2_LOCK}" files "${NAME}")
    file(SHA256 "${XML2_SOURCE}/${NAME}" ACTUAL)
    if(NOT ACTUAL STREQUAL EXPECTED)
        message(FATAL_ERROR "Pinned XML source mismatch: ${NAME}")
    endif()
endforeach()

add_library(nexus-xml2 STATIC
    "${XML2_SOURCE}/HTMLparser.c"
    "${XML2_SOURCE}/HTMLtree.c"
    "${XML2_SOURCE}/SAX.c"
    "${XML2_SOURCE}/SAX2.c"
    "${XML2_SOURCE}/buf.c"
    "${XML2_SOURCE}/catalog.c"
    "${XML2_SOURCE}/chvalid.c"
    "${XML2_SOURCE}/debugXML.c"
    "${XML2_SOURCE}/dict.c"
    "${XML2_SOURCE}/encoding.c"
    "${XML2_SOURCE}/entities.c"
    "${XML2_SOURCE}/error.c"
    "${XML2_SOURCE}/globals.c"
    "${XML2_SOURCE}/hash.c"
    "${XML2_SOURCE}/list.c"
    "${XML2_SOURCE}/parser.c"
    "${XML2_SOURCE}/parserInternals.c"
    "${XML2_SOURCE}/pattern.c"
    "${XML2_SOURCE}/relaxng.c"
    "${XML2_SOURCE}/threads.c"
    "${XML2_SOURCE}/tree.c"
    "${XML2_SOURCE}/uri.c"
    "${XML2_SOURCE}/valid.c"
    "${XML2_SOURCE}/xinclude.c"
    "${XML2_SOURCE}/xmlIO.c"
    "${XML2_SOURCE}/xmlmemory.c"
    "${XML2_SOURCE}/xmlreader.c"
    "${XML2_SOURCE}/xmlregexp.c"
    "${XML2_SOURCE}/xmlsave.c"
    "${XML2_SOURCE}/xmlschemas.c"
    "${XML2_SOURCE}/xmlschemastypes.c"
    "${XML2_SOURCE}/xmlstring.c"
    "${XML2_SOURCE}/xmlunicode.c"
    "${XML2_SOURCE}/xpath.c"
    "${XML2_SOURCE}/xpointer.c")
target_include_directories(nexus-xml2 PUBLIC "${XML2_SOURCE}/include")
target_compile_definitions(nexus-xml2 PUBLIC LIBXML_STATIC PRIVATE HAVE_VA_COPY)
set_target_properties(nexus-xml2 PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
