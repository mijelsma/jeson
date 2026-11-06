if(ESP_PLATFORM)
    idf_component_register(INCLUDE_DIRS "include")
    return()
endif()

cmake_minimum_required(VERSION 3.16)

project(
    jeson
    VERSION 0.2.0
    DESCRIPTION "Lightweight, typed JSON for embedded and desktop C++20"
    HOMEPAGE_URL "https://github.com/mijelsma/jeson"
    LANGUAGES CXX
)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

add_library(jeson INTERFACE)
add_library(jeson::jeson ALIAS jeson)

target_compile_features(jeson INTERFACE cxx_std_20)
target_include_directories(
    jeson
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(JESON_IS_TOP_LEVEL ON)
else()
    set(JESON_IS_TOP_LEVEL OFF)
endif()

option(JESON_BUILD_TESTS "Build the JESON test suite" ${JESON_IS_TOP_LEVEL})
option(JESON_BUILD_EXAMPLES "Build the JESON examples" ${JESON_IS_TOP_LEVEL})

if(JESON_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        add_executable(jeson_tests tests/test_jeson.cpp)
        target_link_libraries(jeson_tests PRIVATE jeson::jeson)
        target_include_directories(jeson_tests PRIVATE tests)
        add_test(NAME jeson.tests COMMAND jeson_tests)

        add_executable(jeson_smoke tests/smoke.cpp)
        target_link_libraries(jeson_smoke PRIVATE jeson::jeson)
        add_test(NAME jeson.smoke COMMAND jeson_smoke)
    endif()
endif()

if(JESON_BUILD_EXAMPLES)
    foreach(example IN ITEMS simple nested_and_arrays enum validator writer errors patch advanced value compact_document)
        add_executable(jeson_example_${example} examples/${example}_example.cpp)
        target_link_libraries(jeson_example_${example} PRIVATE jeson::jeson)
    endforeach()
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
    TARGETS jeson
    EXPORT jesonTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
    EXPORT jesonTargets
    FILE jesonTargets.cmake
    NAMESPACE jeson::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jeson
)

configure_package_config_file(
    cmake/jesonConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/jesonConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jeson
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/jesonConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)
configure_file(cmake/jeson.pc.in jeson.pc @ONLY)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/jesonConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/jesonConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jeson
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/jeson.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
install(
    FILES LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/jeson
)
