project(capi)

file(GLOB _src *.h *.cpp)

add_library(${PROJECT_NAME} ${_src})

target_link_libraries(${PROJECT_NAME} PUBLIC acetree)