include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(prodjlink_tests
    test_packet_parsing.cpp
    test_dj_state.cpp
    test_reconnect.cpp
)

target_link_libraries(prodjlink_tests
    PRIVATE prodjlink Catch2::Catch2WithMain
)

target_include_directories(prodjlink_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

include(CTest)
include(Catch)
catch_discover_tests(prodjlink_tests)
