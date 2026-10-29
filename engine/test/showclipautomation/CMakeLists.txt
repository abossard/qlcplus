add_executable(showclipautomation_test WIN32
    showclipautomation_test.cpp showclipautomation_test.h
)
target_include_directories(showclipautomation_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(showclipautomation_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
