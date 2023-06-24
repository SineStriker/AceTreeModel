# Copied from ChorusKit
function(_parse_version _prefix _version)
    string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ ${_version})

    foreach(_i RANGE 1 4)
        if(${CMAKE_MATCH_COUNT} GREATER_EQUAL ${_i})
            set(_tmp ${CMAKE_MATCH_${_i}})
        else()
            set(_tmp 0)
        endif()

        set(${_prefix}_${_i} ${_tmp} PARENT_SCOPE)
    endforeach()
endfunction()

_parse_version(_version ${PROJECT_VERSION})

# No Icon
set(RC_ICON_COMMENT "//")
set(RC_ICON_PATH)

# Metadata
set(RC_VERSION ${_version_1},${_version_2},${_version_3},${_version_4})
set(RC_APPLICATION_NAME ${PROJECT_NAME})
set(RC_VERSION_STRING ${PROJECT_VERSION})
set(RC_DESCRIPTION "Qt Journaling Tree Model")
set(RC_COPYRIGHT "Copyright 2022-2023 SineStriker")

# Generate rc file
set(_rc_path ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_res.rc)
configure_file(${CMAKE_CURRENT_LIST_DIR}/WinResource.rc.in ${_rc_path} @ONLY)

# Add source
target_sources(${PROJECT_NAME} PRIVATE ${_rc_path})
