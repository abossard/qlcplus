add_executable(audioprofile_test WIN32
    audioprofile_test.cpp audioprofile_test.h
)

target_include_directories(audioprofile_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../../audio/src
)

target_link_libraries(audioprofile_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
    qlcplusaudio
)
