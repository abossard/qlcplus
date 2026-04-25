add_executable(beatquantize_test WIN32
    beatquantize_test.cpp beatquantize_test.h
)
target_include_directories(beatquantize_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(beatquantize_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
