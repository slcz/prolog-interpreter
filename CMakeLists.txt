cmake_minimum_required (VERSION 3.3)
set (CMAKE_CXX_COMPILER "/usr/bin/clang++-5.0")
project (prolog)
set (CMAKE_CXX_FLAGS "-std=c++17 -Wall -Werror -O2 -g")
set (prolog_VERSION_MAJOR 0)
set (prolog_VERSION_MINOR 1)
add_executable(prolog main.c++ unification.c++ parser.c++
	parser.h unique-id.h unification-test.c++ interpreter.c++ interpreter.h
	unification.h)
