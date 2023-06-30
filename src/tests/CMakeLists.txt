find_package(QT NAMES Qt6 Qt5 COMPONENTS Test REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test REQUIRED)

function(add_test _target)
    add_executable(${_target} ${ARGN})

    target_link_libraries(${_target} PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        acetree
    )

    set_target_properties(${_target} PROPERTIES AUTOMOC on)
endfunction()

add_test(tst_Basic tst_Basic.cpp)
