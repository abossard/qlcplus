add_executable(mixlayer_test WIN32
    ../common/resource_paths.h
    mixlayer_test.cpp mixlayer_test.h
)
target_include_directories(mixlayer_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(mixlayer_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
