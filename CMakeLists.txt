###########################
# Init
##########################
cmake_minimum_required(VERSION 3.5)
set(MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.cmake/Modules")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${MODULE_DIR})
include(LazyUtils)
include(msvc)
include(gcc)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED true)

project(test_exe LANGUAGES CXX)

msvc_remove_warning_level()

###########################
# Output dir
##########################
set_output_dir("${CMAKE_BINARY_DIR}/dist")

msvc_set_flags_if("/std:c++latest  /utf-8")

msvc_set_flags_if("/W4")
gcc_set_flags_if("-Wall -Wextra")

add_subdirectory("src/")

###########################
# Linter
##########################
option(CLANG_TIDY "Enable Clang tidy checks" OFF)
if (CLANG_TIDY)
    include(ClangTidy)
    enable_clang_tidy_for(dq_thread_safe_conn)
endif()
