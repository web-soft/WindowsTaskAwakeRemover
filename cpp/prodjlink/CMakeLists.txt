cmake_minimum_required(VERSION 3.16)
project(prodjlink VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PRODJLINK_BUILD_TESTS    "Build unit tests"       ON)
option(PRODJLINK_BUILD_EXAMPLES "Build examples"         ON)
option(PRODJLINK_INSTALL        "Generate install rules" ON)

add_library(prodjlink
    src/device_announcement.cpp
    src/beat.cpp
    src/cdj_status.cpp
    src/mixer_status.cpp
    src/device_finder.cpp
    src/beat_finder.cpp
    src/virtual_cdj.cpp
    src/metadata_client.cpp
)

target_include_directories(prodjlink
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(prodjlink PUBLIC cxx_std_17)

# Platform-specific socket and network interface implementations
if(WIN32)
    target_sources(prodjlink PRIVATE
        src/platform/socket_windows.cpp
        src/platform/network_interface_windows.cpp
    )
    target_link_libraries(prodjlink PRIVATE ws2_32 iphlpapi)
elseif(APPLE)
    target_sources(prodjlink PRIVATE
        src/platform/socket_posix.cpp
        src/platform/network_interface_mac.cpp
    )
elseif(UNIX)
    target_sources(prodjlink PRIVATE
        src/platform/socket_posix.cpp
        src/platform/network_interface_linux.cpp
    )
endif()

find_package(Threads REQUIRED)
target_link_libraries(prodjlink PRIVATE Threads::Threads)

if(PRODJLINK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(PRODJLINK_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(PRODJLINK_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS prodjlink EXPORT prodjlinkTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(DIRECTORY include/prodjlink DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT prodjlinkTargets
        FILE ProDJLinkTargets.cmake
        NAMESPACE prodjlink::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/prodjlink
    )
    configure_package_config_file(
        cmake/ProDJLinkConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/ProDJLinkConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/prodjlink
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/ProDJLinkConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ProDJLinkConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ProDJLinkConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/prodjlink
    )
endif()
