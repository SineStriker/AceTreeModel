# Enable Qt auto generator
set(CMAKE_AUTOMOC on)
set(CMAKE_AUTOUIC on)
set(CMAKE_AUTORCC on)

# Add target
if(ACETREE_BUILD_STATIC)
    add_library(${PROJECT_NAME} STATIC)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ACETREE_STATIC)
else()
    add_library(${PROJECT_NAME} SHARED)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE ACETREE_LIBRARY)

if(ACETREE_ENABLE_DEBUG)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ACETREE_ENABLE_DEBUG)
endif()

if(NOT WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
endif()

# Add source files
file(GLOB_RECURSE _src include/*.h src/*.h src/*.cpp)

target_sources(${PROJECT_NAME} PRIVATE ${_headers} ${_src})

# Handle libraries and include directories
qtmediate_link_qt_libraries(${PROJECT_NAME} PUBLIC Core)

target_include_directories(${PROJECT_NAME} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/entity
    src/itemmodel
    src/serialization
    include/private
)

# Set extra
if(WIN32 AND NOT ACETREE_BUILD_STATIC)
    qtmediate_add_win_rc(${PROJECT_NAME}
        DESCRIPTION "Qt Journaling Tree Model"
        COPYRIGHT "Copyright 2022-2023 SineStriker"
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME}-qt${QT_VERSION_MAJOR})

# Add install target
if(ACETREE_INSTALL)
    target_include_directories(${PROJECT_NAME} PUBLIC
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${ACETREE_INSTALL_NAME}>"
    )

    install(TARGETS ${ACETREE_INSTALL_NAME}
        EXPORT ${ACETREE_INSTALL_NAME}Targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${ACETREE_INSTALL_NAME}

        # FILES_MATCHING PATTERN "*.h"
    )
endif()
