project(atec VERSION 0.0.1.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTOUIC on)
set(CMAKE_AUTORCC on)

file(GLOB _src *.h *.cpp)

add_executable(${PROJECT_NAME} ${_src})

target_link_libraries(${PROJECT_NAME} PRIVATE acetree)

# Set extra
if(WIN32)
    set(RC_DESCRIPTION "AceTreeEntity Wrapper Generator")
    include(${ACETREE_MODULES_DIR}/winrc.cmake)
endif()