configure_file(Nexus/MicrosoftGame.config "${CMAKE_BINARY_DIR}/bin/MicrosoftGame.config" COPYONLY)

add_executable(test_xgameruntime
        Nexus/test_system.cpp
        Nexus/test_com_ownership.cpp
        Nexus/test_optional_providers.cpp
        Nexus/test_connectivity.cpp
        Nexus/test_package_layout.cpp
        Nexus/test_async_results.cpp
        Nexus/test_thread_local.cpp
        Nexus/test_async_wait.cpp
        System/test_xthreading.cpp
        test_fixtures.cpp)

target_link_libraries(test_xgameruntime
        PRIVATE
        libxgameruntime
        GTest::gtest_main ws2_32 iphlpapi )

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

add_executable(test_nexus_timer Nexus/test_timer.cpp)
target_link_libraries(test_nexus_timer PRIVATE xgameruntime-impl GTest::gtest_main)
target_compile_features(test_nexus_timer PRIVATE cxx_std_23)
target_link_options(test_nexus_timer PRIVATE -static -static-libgcc -static-libstdc++)
set_target_properties(test_nexus_timer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(test_nexus_save Nexus/test_save.cpp)
target_link_libraries(test_nexus_save PRIVATE xgameruntime-impl GTest::gtest_main)
target_compile_features(test_nexus_save PRIVATE cxx_std_23)
target_link_options(test_nexus_save PRIVATE -static -static-libgcc -static-libstdc++)
set_target_properties(test_nexus_save PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(test_nexus_user Nexus/test_user.cpp)
target_link_libraries(test_nexus_user PRIVATE xgameruntime-impl GTest::gtest_main)
target_compile_features(test_nexus_user PRIVATE cxx_std_23)
target_link_options(test_nexus_user PRIVATE -static -static-libgcc -static-libstdc++)
set_target_properties(test_nexus_user PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(test_nexus_configuration Nexus/test_configuration.cpp)
target_link_libraries(test_nexus_configuration PRIVATE xgameruntime-impl GTest::gtest_main)
target_compile_features(test_nexus_configuration PRIVATE cxx_std_23)
target_link_options(test_nexus_configuration PRIVATE -static -static-libgcc -static-libstdc++)
set_target_properties(test_nexus_configuration PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

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
    foreach(probe runtime-init async-schedule async-completion-lifetime taskqueue-teardown taskqueue-contract taskqueue-close)
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

foreach(probe sync transfer)
    add_executable(nexus_gamesave-${probe} Nexus/gamesave-${probe}.cpp)
    target_link_libraries(nexus_gamesave-${probe} PRIVATE xgameruntime-impl)
    target_compile_features(nexus_gamesave-${probe} PRIVATE cxx_std_23)
    target_link_options(nexus_gamesave-${probe} PRIVATE -static -static-libgcc -static-libstdc++)
    set_target_properties(nexus_gamesave-${probe} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endforeach()

add_executable(nexus_gamesave-files Nexus/gamesave-files.cpp)
target_link_libraries(nexus_gamesave-files PRIVATE xgameruntime-impl)
target_compile_features(nexus_gamesave-files PRIVATE cxx_std_23)
target_link_options(nexus_gamesave-files PRIVATE -static -static-libgcc -static-libstdc++)
set_target_properties(nexus_gamesave-files PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NEXUS_MIGRATION_PROBES)
    foreach(probe package-identity package-legacy package-chunks package-monitors package-progress-bytes)
        set(package_probe_source "Nexus/${probe}-probe.c")
        if(probe STREQUAL "package-legacy" AND CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(package_probe_source "Nexus/package-legacy-32-probe.c")
        endif()
        add_executable(nexus_${probe} ${package_probe_source})
        add_dependencies(nexus_${probe} xgameruntime-idl)
        target_compile_definitions(nexus_${probe} PRIVATE __WINESRC__)
        target_include_directories(nexus_${probe} PRIVATE "${IDL_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include")
        set_target_properties(nexus_${probe} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    endforeach()
endif()

foreach(probe store-entitlements package-content)
    add_executable(nexus_${probe} Nexus/${probe}.cpp)
    add_dependencies(nexus_${probe} xgameruntime-idl)
    target_include_directories(nexus_${probe} PRIVATE "${IDL_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(nexus_${probe} PRIVATE nexus-xml2 xgameruntime-common shell32 bcrypt crypt32 advapi32 runtimeobject ole32 winhttp wininet ws2_32)
    target_compile_features(nexus_${probe} PRIVATE cxx_std_23)
    target_link_options(nexus_${probe} PRIVATE -static -static-libgcc -static-libstdc++)
    set_target_properties(nexus_${probe} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endforeach()
target_link_libraries(nexus_package-content PRIVATE xgameruntime-impl)

if(NEXUS_MIGRATION_PROBES)
    foreach(probe game-relaunch async-provider-contract)
        add_executable(nexus_${probe} Nexus/${probe}-probe.c)
        add_dependencies(nexus_${probe} xgameruntime-idl)
        target_compile_definitions(nexus_${probe} PRIVATE __WINESRC__)
        target_include_directories(nexus_${probe} PRIVATE "${IDL_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/include")
        set_target_properties(nexus_${probe} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    endforeach()
    target_link_options(nexus_game-relaunch PRIVATE -municode)
endif()
