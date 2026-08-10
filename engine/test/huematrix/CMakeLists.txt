add_executable(huematrix_test WIN32
    ../common/resource_paths.h
    huematrix_test.cpp huematrix_test.h
)
target_include_directories(huematrix_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../mastertimer
)

# AC17 asserts on the shipped icon artifacts, which are not copied into the
# test's runtime resource directory.
target_compile_definitions(huematrix_test PRIVATE
    QLCPLUS_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(huematrix_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)

if(qmlui OR (QT_VERSION_MAJOR GREATER 5))
    target_link_libraries(huematrix_test PRIVATE
        Qt${QT_MAJOR_VERSION}::Qml
    )
endif()

if(NOT (qmlui OR (QT_VERSION_MAJOR GREATER 5)))
    target_link_libraries(huematrix_test PRIVATE
        Qt${QT_MAJOR_VERSION}::Script
    )
endif()

# Consider using qt_generate_deploy_app_script() for app deployment if
# the project can use Qt 6.3. In that case rerun qmake2cmake with
# --min-qt-version=6.3.
