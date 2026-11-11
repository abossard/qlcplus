add_executable(hotplugmonitor_test WIN32 MACOSX_BUNDLE
    hpmtest.cpp hpmtest.h
    main.cpp
)
set_target_properties(hotplugmonitor_test PROPERTIES OUTPUT_NAME test)
target_include_directories(hotplugmonitor_test PRIVATE
    ../src
)

target_link_libraries(hotplugmonitor_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Widgets
    hotplugmonitor
)

if(iokit)
    target_link_libraries(hotplugmonitor_test PRIVATE
        "-framework CoreFoundation"
        "-framework IOKit"
    )
endif()

# Consider using qt_generate_deploy_app_script() for app deployment if
# the project can use Qt 6.3. In that case rerun qmake2cmake with
# --min-qt-version=6.3.
