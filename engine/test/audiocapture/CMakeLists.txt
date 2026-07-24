add_executable(audiocapture_test WIN32
    audiocapture_test.cpp audiocapture_test.h
)
target_include_directories(audiocapture_test PRIVATE
    ../../../plugins/interfaces
    ../../audio/src
    ../../src
)

target_link_libraries(audiocapture_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
