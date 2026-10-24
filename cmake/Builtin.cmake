# Native component builds remain ordinary PE DLLs. A Proton distribution marks
# its module with the winebuild executable from that same Wine build tree.
option( NEXUS_WINE_BUILTIN "Build a Wine builtin PE module" OFF )
set( NEXUS_WINEBUILD "" CACHE FILEPATH "Matching Wine build's host winebuild executable" )

if( NEXUS_WINE_BUILTIN )
    if( NOT IS_ABSOLUTE "${NEXUS_WINEBUILD}" OR NOT EXISTS "${NEXUS_WINEBUILD}" )
        message( FATAL_ERROR "NEXUS_WINE_BUILTIN requires an explicit matching NEXUS_WINEBUILD executable" )
    endif()

    add_custom_command( TARGET xgameruntime POST_BUILD
            COMMAND "${NEXUS_WINEBUILD}" --builtin "$<TARGET_FILE:xgameruntime>"
            COMMAND "${CMAKE_COMMAND}" "-DMODULE=$<TARGET_FILE:xgameruntime>"
                    -P "${PROJECT_SOURCE_DIR}/cmake/CheckBuiltin.cmake"
            VERBATIM )
endif()
