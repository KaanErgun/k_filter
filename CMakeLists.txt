# Optional, additive convenience — the canonical integration is still
# "copy include/k_filter.h + src/k_filter.c into your project".
#
#   add_subdirectory(k_filter)      # or FetchContent
#   target_link_libraries(app PRIVATE k_filter)
cmake_minimum_required(VERSION 3.14)
project(k_filter VERSION 2.0.0 LANGUAGES C)

add_library(k_filter STATIC src/k_filter.c)
target_include_directories(k_filter PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(k_filter PUBLIC c_std_99)

option(K_FILTER_BUILD_TESTS "Build k_filter unit tests" OFF)
if(K_FILTER_BUILD_TESTS)
    enable_testing()
    add_executable(k_filter_tests test/test_filters.c)
    target_include_directories(k_filter_tests PRIVATE test)
    target_link_libraries(k_filter_tests PRIVATE k_filter m)
    add_test(NAME k_filter_tests COMMAND k_filter_tests)
endif()
