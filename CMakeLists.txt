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
	/usr/lib/llvm-18/lib/liblldELF.a
	/usr/lib/llvm-18/lib/liblldCommon.a
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

function(add_ccc_integration_test test_name expected_exit)
	set(options)
	set(oneValueArgs)
	set(multiValueArgs SOURCES COMPILE_ARGS)
	cmake_parse_arguments(T "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	if(NOT T_SOURCES)
		message(FATAL_ERROR "add_ccc_integration_test(${test_name}) requires SOURCES")
	endif()

	set(abs_sources)
	foreach(rel_source IN LISTS T_SOURCES)
		list(APPEND abs_sources "${CMAKE_SOURCE_DIR}/${rel_source}")
	endforeach()

	set(out_bin "${CMAKE_BINARY_DIR}/${test_name}_app")
	add_test(NAME ${test_name}_compile
		COMMAND $<TARGET_FILE:ccc> ${abs_sources} ${T_COMPILE_ARGS} -o ${out_bin})
	add_test(NAME ${test_name}_run
		COMMAND /bin/bash -c "\"${out_bin}\" >/dev/null 2>&1; test $? -eq ${expected_exit}")
	set_tests_properties(${test_name}_run PROPERTIES DEPENDS ${test_name}_compile)
endfunction()

function(add_ccc_invalid_test test_name source_relpath)
	set(out_bin "${CMAKE_BINARY_DIR}/${test_name}_invalid_app")
	add_test(NAME ${test_name}_invalid_compile
		COMMAND $<TARGET_FILE:ccc> ${CMAKE_SOURCE_DIR}/${source_relpath} -o ${out_bin})
	set_tests_properties(${test_name}_invalid_compile PROPERTIES WILL_FAIL TRUE)
endfunction()

# integration/return
add_ccc_integration_test(ret_basic_return 1
	SOURCES tests/integration/return/basic_return.c)

# integration/preprocess
add_ccc_integration_test(pp_comment 2
	SOURCES tests/integration/preprocess/comment.c)
add_ccc_integration_test(pp_if_elif 1
	SOURCES tests/integration/preprocess/if_elif.c)
add_ccc_integration_test(pp_macro_value 0
	SOURCES tests/integration/preprocess/macro_value.c
	COMPILE_ARGS -DVALUE=42)

# integration/emit
add_ccc_integration_test(emit_simple 0
	SOURCES tests/integration/emit/simple.c tests/integration/emit/helper.c)

# integration/loop
add_ccc_integration_test(loop_for_nested_if 0
	SOURCES tests/integration/loop/for_nested_if.c)

# integration/multi_file
add_ccc_integration_test(multi_test1 0
	SOURCES tests/integration/multi_file/test1/multi_file_main.c
			tests/integration/multi_file/test1/multi_file_helper.c)
add_ccc_integration_test(multi_test2 40
	SOURCES tests/integration/multi_file/test2/main.c
			tests/integration/multi_file/test2/test.c)

# integration/std
set(CCC_STD_TEST_DEFINES -D__x86_64__=1 -D__LP64__=1)
add_ccc_integration_test(std_ctype 0
	SOURCES tests/integration/std/ctype.c
	COMPILE_ARGS ${CCC_STD_TEST_DEFINES})
add_ccc_integration_test(std_errno 0
	SOURCES tests/integration/std/errno.c
	COMPILE_ARGS ${CCC_STD_TEST_DEFINES})
add_ccc_integration_test(std_limits 8
	SOURCES tests/integration/std/limits.c
	COMPILE_ARGS ${CCC_STD_TEST_DEFINES})
add_ccc_integration_test(std_stdarg 0
	SOURCES tests/integration/std/stdarg.c
	COMPILE_ARGS ${CCC_STD_TEST_DEFINES})
add_ccc_integration_test(std_stddef 0
	SOURCES tests/integration/std/stddef.c
	COMPILE_ARGS ${CCC_STD_TEST_DEFINES})
add_ccc_integration_test(std_stdio 1
	SOURCES tests/integration/std/stdio.c
	COMPILE_ARGS ${CCC_STD_TEST_DEFINES})
add_ccc_integration_test(std_stdlib 0
	SOURCES tests/integration/std/stdlib.c
	COMPILE_ARGS ${CCC_STD_TEST_DEFINES})
add_ccc_integration_test(std_string 0
	SOURCES tests/integration/std/string.c
	COMPILE_ARGS ${CCC_STD_TEST_DEFINES})
add_ccc_integration_test(std_variadic_decl_call 0
	SOURCES tests/integration/std/variadic_decl_call.c
	COMPILE_ARGS ${CCC_STD_TEST_DEFINES})

# integration/type
add_ccc_integration_test(type_array 0
	SOURCES tests/integration/type/array_type.c)
add_ccc_integration_test(type_char 0
	SOURCES tests/integration/type/char.c)
add_ccc_integration_test(type_enum 0
	SOURCES tests/integration/type/enum_type.c)
add_ccc_integration_test(type_float_double 0
	SOURCES tests/integration/type/float_double.c)
add_ccc_integration_test(type_function_pointer 0
	SOURCES tests/integration/type/function_pointer_type.c)
add_ccc_integration_test(type_pointer_levels 0
	SOURCES tests/integration/type/pointer_levels.c)
add_ccc_integration_test(type_pointer_void 0
	SOURCES tests/integration/type/pointer_void.c)
add_ccc_integration_test(type_struct_member_dot 0
	SOURCES tests/integration/type/struct_member_dot.c)
add_ccc_integration_test(type_struct_member_arrow 0
	SOURCES tests/integration/type/struct_member_arrow.c)
add_ccc_integration_test(type_struct 0
	SOURCES tests/integration/type/struct_type.c)
add_ccc_integration_test(type_union 0
	SOURCES tests/integration/type/union_type.c)

# invalid
add_ccc_invalid_test(invalid_conflicting_prototype
	tests/invalid/conflicting_prototype.c)
add_ccc_invalid_test(invalid_undeclared_var
	tests/invalid/undeclared_var.c)
