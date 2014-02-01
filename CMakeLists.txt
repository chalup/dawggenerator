cmake_minimum_required (VERSION 2.6)
project (dawggenerator)
add_executable (dawggenerator dawggenerator.cpp sha1.c dawgminify.c)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  list(APPEND CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++")
elseif(CMAKE_COMPILER_IS_GNUCXX)
  list(APPEND CMAKE_CXX_FLAGS "-O2 -std=c++0x -U__STRICT_ANSI__ -Wall -Werror")
endif()

