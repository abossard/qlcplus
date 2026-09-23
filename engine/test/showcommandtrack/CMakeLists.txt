add_executable(showcommandtrack_test WIN32
    showcommandtrack_test.cpp showcommandtrack_test.h
)
target_include_directories(showcommandtrack_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(showcommandtrack_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
