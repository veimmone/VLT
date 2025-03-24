cmake_minimum_required(VERSION 3.14)

project(PSEQ)

set(CMAKE_CXX_STANDARD 23)

add_library(PSEQ STATIC
  fseq_v2.cpp
  fseq_v2.h
)

option(BUILD_TESTING "Build the VLT test suite" OFF)
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.0
    )
    FetchContent_MakeAvailable(catch2)

    add_executable(test_vlt
      test/fseq_v2.cpp
    )
    target_link_libraries(test_vlt PRIVATE
      PSEQ
      Catch2::Catch2WithMain
    )
    target_include_directories(test_vlt PRIVATE ${CMAKE_CURRENT_LIST_DIR})

    enable_testing()
    add_test(NAME VLT_Test COMMAND test_vlt)
endif()