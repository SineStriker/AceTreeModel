project(modelcli)

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTOUIC on)
set(CMAKE_AUTORCC on)

file(GLOB _src *.h *.cpp)

add_executable(${PROJECT_NAME} ${_src})

target_link_libraries(${PROJECT_NAME} PRIVATE acetree)

qtmediate_link_qt_libraries(${PROJECT_NAME} PUBLIC Core)

if(NOT WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
endif()
