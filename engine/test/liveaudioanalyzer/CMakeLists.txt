add_executable(liveaudioanalyzer_test WIN32
    liveaudioanalyzer_test.cpp liveaudioanalyzer_test.h
)

target_include_directories(liveaudioanalyzer_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../../audio/src
)

target_link_libraries(liveaudioanalyzer_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusaudio
)
