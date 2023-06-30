cmake_minimum_required(VERSION 3.17)

project(acetree VERSION 0.0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(ACETREE_BUILD_STATIC "Build static library" off)
option(ACETREE_BUILD_TESTS "Build test cases" on)
option(ACETREE_BUILD_EXAMPLES "Build example targets" on)
option(ACETREE_BUILD_TOOLS "Build acetree tools" on)
option(ACETREE_INSTALL "Add install target" on)

# CMake Configurations
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(ACETREE_MODULES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(ACETREE_INSTALL_NAME acetree)

if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /manifest:no")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
endif()

add_subdirectory(src)