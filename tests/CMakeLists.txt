# Unit tests for the platform-independent core library.
#
# GoogleTest is the only third-party dependency and is used only here.
# An installed copy is preferred (offline builds, vcpkg, distro packages);
# otherwise a pinned release is downloaded at configure time.

find_package(GTest 1.11 CONFIG QUIET)
if(GTest_FOUND)
    message(STATUS "sysdiag: using installed GoogleTest ${GTest_VERSION}")
else()
    message(STATUS "sysdiag: GoogleTest not found - fetching v1.15.2")
    include(FetchContent)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # match the default MSVC runtime
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(sysdiag_tests
    cli_test.cpp
    cpu_usage_test.cpp
    formatters_test.cpp
    hw_names_test.cpp
    json_writer_test.cpp
    report_builder_test.cpp
    text_test.cpp
    units_test.cpp)
target_include_directories(sysdiag_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(sysdiag_tests PRIVATE sysdiag_core GTest::gtest_main)
sysdiag_apply_compiler_options(sysdiag_tests)

include(GoogleTest)
gtest_discover_tests(sysdiag_tests DISCOVERY_MODE PRE_TEST)
