
if(NOT DEFINED QTMEDIATE_CMAKE_MODULES_DIR)
    message(FATAL_ERROR "QTMEDIATE_CMAKE_MODULES_DIR not defined!")
endif()

include("${QTMEDIATE_CMAKE_MODULES_DIR}/QtMediateAPI.cmake")

add_subdirectory(acetree)

# Add test cases
if(ACETREE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Add examples
if(ACETREE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install
if(ACETREE_INSTALL)
    # Add install target
    set(_install_dir ${CMAKE_INSTALL_LIBDIR}/cmake/${ACETREE_INSTALL_NAME})

    # Add version file
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${ACETREE_INSTALL_NAME}ConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    # Add configuration file
    configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/${ACETREE_INSTALL_NAME}Config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/${ACETREE_INSTALL_NAME}Config.cmake"
        INSTALL_DESTINATION ${_install_dir}
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
    )

    # Install cmake files
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${ACETREE_INSTALL_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${ACETREE_INSTALL_NAME}ConfigVersion.cmake"
        DESTINATION ${_install_dir}
    )

    # Install cmake targets files
    install(EXPORT ${ACETREE_INSTALL_NAME}Targets
        DESTINATION ${_install_dir}
    )
endif()