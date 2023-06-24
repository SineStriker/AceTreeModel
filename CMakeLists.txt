cmake_minimum_required(VERSION 3.17)

project(acetree VERSION 0.0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt auto generator
set(CMAKE_AUTOMOC on)
set(CMAKE_AUTOUIC on)
set(CMAKE_AUTORCC on)

# Options
option(ACETREE_BUILD_TESTS "Build test cases" on)
option(ACETREE_BUILD_STATIC "Build static library" off)
option(ACETREE_INSTALL "Add install target" on)

# Output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Add library
find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)

if(ACETREE_BUILD_STATIC)
    add_library(${PROJECT_NAME} STATIC)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ACETREE_STATIC)
else()
    add_library(${PROJECT_NAME} SHARED)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE ACETREE_LIBRARY)

# Add source files
file(GLOB_RECURSE _headers include/*.h)
file(GLOB_RECURSE _sources src/*.h src/*.cpp)

target_sources(${PROJECT_NAME} PRIVATE ${_headers} ${_sources})

# Handle libraries and include directories
target_link_libraries(${PROJECT_NAME} PUBLIC Qt${QT_VERSION_MAJOR}::Core)

target_include_directories(${PROJECT_NAME} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/entity
    src/itemmodel
    src/serialization
    src/tools
    src/widgets
    include/private
)

# Set extra
if(WIN32 AND NOT ACETREE_BUILD_STATIC)
    include(res/winrc.cmake)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME}-qt${QT_VERSION_MAJOR})

# Add test cases
if(ACETREE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Add install target
if(ACETREE_INSTALL)
    set(ACETREE_INSTALL_NAME acetree)
    target_include_directories(${PROJECT_NAME} PUBLIC
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${ACETREE_INSTALL_NAME}>"
    )
    include(res/install.cmake)
endif()