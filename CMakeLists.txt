cmake_minimum_required(VERSION 3.14)

project(PSEQ)

set(CMAKE_CXX_STANDARD 23)

add_library(PSEQ STATIC
  pseq.cpp
  pseq.h
  fseq_v2.cpp
  fseq_v2.h
)

add_executable(pseq_test main.cpp)
target_link_libraries(pseq_test PSEQ)