add_executable(rgbscript_test WIN32
    ../common/resource_paths.h
    rgbscript_test.cpp rgbscript_test.h
)
target_include_directories(rgbscript_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../mastertimer
)

target_link_libraries(rgbscript_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
target_compile_definitions(rgbscript_test PRIVATE
    RGBSCRIPTS_SOURCE_DIR="${CMAKE_SOURCE_DIR}/resources/rgbscripts")

if(APPLE)
    set(RGBSCRIPT_TEST_RESOURCE_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/../Resources/RGBScripts")
    file(MAKE_DIRECTORY "${RGBSCRIPT_TEST_RESOURCE_DIR}")
    if(NOT EXISTS "${RGBSCRIPT_TEST_RESOURCE_DIR}/hsvutil.js")
        file(CREATE_LINK
            "${CMAKE_SOURCE_DIR}/resources/rgbscripts/hsvutil.js"
            "${RGBSCRIPT_TEST_RESOURCE_DIR}/hsvutil.js"
            SYMBOLIC COPY_ON_ERROR)
    endif()
endif()

if(qmlui OR (QT_VERSION_MAJOR GREATER 5))
    target_link_libraries(rgbscript_test PRIVATE
        Qt${QT_MAJOR_VERSION}::Qml
    )
endif()

if(NOT (qmlui OR (QT_VERSION_MAJOR GREATER 5)))
    target_link_libraries(rgbscript_test PRIVATE
        Qt${QT_MAJOR_VERSION}::Script
    )
endif()

# Consider using qt_generate_deploy_app_script() for app deployment if
# the project can use Qt 6.3. In that case rerun qmake2cmake with
# --min-qt-version=6.3.
