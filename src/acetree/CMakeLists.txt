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
    include/io
)

# Set extra
if(WIN32 AND NOT ACETREE_BUILD_STATIC)
    set(RC_DESCRIPTION "Qt Journaling Tree Model")
    include(${ACETREE_MODULES_DIR}/winrc.cmake)
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
