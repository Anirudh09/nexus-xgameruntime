# Generate required WinRT declarations from the matching Wine source, including
# SDKs without a pre-generated Windows.Data.Json header. No API stub/fallback.
file(READ "${CMAKE_CURRENT_LIST_DIR}/winrt.lock.json" WINRT_LOCK)
string(JSON WINRT_COUNT LENGTH "${WINRT_LOCK}" files)
math(EXPR WINRT_LAST "${WINRT_COUNT} - 1")
set(WINRT_INPUTS)
foreach(INDEX RANGE ${WINRT_LAST})
    string(JSON NAME MEMBER "${WINRT_LOCK}" files ${INDEX})
    string(JSON EXPECTED GET "${WINRT_LOCK}" files "${NAME}")
    set(FILE "${NEXUS_WINE_SOURCE}/include/${NAME}")
    file(SHA256 "${FILE}" ACTUAL)
    if(NOT ACTUAL STREQUAL EXPECTED)
        message(FATAL_ERROR "Pinned WinRT source mismatch: ${NAME}")
    endif()
    list(APPEND WINRT_INPUTS "${FILE}")
endforeach()
set(WINRT_JSON_HEADER "${IDL_BINARY_DIR}/windows.data.json.h")
add_custom_command(OUTPUT "${WINRT_JSON_HEADER}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${IDL_BINARY_DIR}"
    COMMAND ${WIDL} ${WIDL_ARCH} -I "${NEXUS_WINE_SOURCE}/include" -h -o "${WINRT_JSON_HEADER}"
        "${NEXUS_WINE_SOURCE}/include/windows.data.json.idl"
    DEPENDS ${WINRT_INPUTS}
    VERBATIM)
list(APPEND IDL_GENERATED_FILES "${WINRT_JSON_HEADER}")
