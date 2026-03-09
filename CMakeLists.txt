#
# ccc
# Copyright (c) 2026 lrisguan <lrisguan@outlook.com>
# 
# This program is released under the terms of the  GNU General Public License version 2(GPLv2)..
# See https://opensource.org/licenses/GPL-2.0 for more information.
# 
# Project homepage: https://github.com/lrisguan/ccc
# Description: A frontend of C
#

cmake_minimum_required(VERSION 3.20)
project(ccc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CCC_ENABLE_WARNINGS "Enable strict compiler warnings" ON)

set(LLVM_DIR "/usr/lib/llvm-18/lib/cmake/llvm" CACHE PATH "Path to LLVMConfig.cmake" FORCE)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_executable(ccc
	src/AST.cc
	src/Compile.cc
	src/Lexer.cc
	src/Link.cc
	src/Parser.cc
	src/Preprocess.cc
	src/SemanticAnalyzer.cc
	src/SymbolTable.cc
	src/IRGenerator.cc
	src/error.cc
	src/ccc.cc
)

target_include_directories(ccc
	PRIVATE
		include
)

target_include_directories(ccc
	SYSTEM PRIVATE
		${LLVM_INCLUDE_DIRS}
		/usr/lib/llvm-18/include
)

target_compile_definitions(ccc PRIVATE ${LLVM_DEFINITIONS})

if (CCC_ENABLE_WARNINGS)
	if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
		target_compile_options(ccc PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-error=cpp)
	endif()
endif()

target_link_libraries(ccc PRIVATE
	/usr/lib/llvm-18/lib/libLLVM-18.so
	/usr/lib/llvm-18/lib/liblldCommon.a
	/usr/lib/llvm-18/lib/liblldELF.a
	zstd
	z
	dl
	m
	pthread
	rt
)

enable_testing()
add_test(NAME ccc_help COMMAND ccc)
set_tests_properties(ccc_help PROPERTIES WILL_FAIL TRUE)
