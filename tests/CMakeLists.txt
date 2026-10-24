add_executable(test_xgameruntime
        Nexus/test_system.cpp
        System/test_xthreading.cpp
        test_fixtures.cpp)

target_link_libraries(test_xgameruntime
        PRIVATE
        libxgameruntime
        GTest::gtest_main )

target_link_options(test_xgameruntime PRIVATE
        -static
        -static-libgcc
        -static-libstdc++ )

target_compile_features(test_xgameruntime
        PUBLIC
        cxx_std_23 )

include( GoogleTest )

target_include_directories(test_xgameruntime
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/external/googletest/googletest/include )

# For discovery purposes.
set_target_properties(test_xgameruntime PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" )

if( CMAKE_CROSSCOMPILING )
    message( STATUS "We are cross compiling tests" )
    find_program( WINE_EXECUTABLE wine )
    message( STATUS "Using wine: ${WINE_EXECUTABLE}" )
    if( NOT WINE_EXECUTABLE )
        message( FATAL_ERROR "Wine is required to run Windows tests on non-Windows hosts" )
    endif()
    set_property(
            TARGET test_xgameruntime
            PROPERTY CROSSCOMPILING_EMULATOR
            "${WINE_EXECUTABLE}" )
endif()

# We should not be pre-running tests during build.
gtest_discover_tests(
        test_xgameruntime
        DISCOVERY_MODE PRE_TEST
        DISCOVERY_TIMEOUT 30 )

add_executable(nexus_system_device_id Nexus/system-device-id-probe.c)
set_target_properties(nexus_system_device_id PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if(CMAKE_CROSSCOMPILING)
    set_property(TARGET nexus_system_device_id PROPERTY CROSSCOMPILING_EMULATOR "${WINE_EXECUTABLE}")
endif()
add_test(NAME NexusSystem.RawDeviceIdContract COMMAND nexus_system_device_id)

option(NEXUS_MIGRATION_PROBES "Build the original Nexus ABI migration regressions" ON)
if(NEXUS_MIGRATION_PROBES)
    foreach(probe async-schedule async-completion-lifetime taskqueue-teardown taskqueue-contract taskqueue-close)
        add_executable(nexus_${probe} Nexus/${probe}-probe.c)
        add_dependencies(nexus_${probe} xgameruntime-idl)
        target_compile_definitions(nexus_${probe} PRIVATE __WINESRC__)
        target_include_directories(nexus_${probe} PRIVATE "${IDL_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include")
        set_target_properties(nexus_${probe} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        if(CMAKE_CROSSCOMPILING)
            set_property(TARGET nexus_${probe} PROPERTY CROSSCOMPILING_EMULATOR "${WINE_EXECUTABLE}")
        endif()
        add_test(NAME NexusMigration.${probe} COMMAND nexus_${probe})
        set_tests_properties(NexusMigration.${probe} PROPERTIES TIMEOUT 45)
    endforeach()
endif()
