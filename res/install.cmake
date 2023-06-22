include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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

install(TARGETS ${ACETREE_INSTALL_NAME}
    EXPORT ${ACETREE_INSTALL_NAME}Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
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

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${ACETREE_INSTALL_NAME}

    # FILES_MATCHING PATTERN "*.h"
)