add_library(xgameruntime-common STATIC
        env.c
        logging.c )

set_target_properties(xgameruntime-common PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)

target_include_directories(xgameruntime-common
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${IDL_BINARY_DIR} )

if(WIN32)
    set_target_properties(xgameruntime-common PROPERTIES
            PREFIX "" )
endif()
