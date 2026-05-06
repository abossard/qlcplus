add_executable(vcaudiotriggers_golden_test WIN32
    vcaudiotriggers_golden_test.cpp vcaudiotriggers_golden_test.h
    ../audioframe/audioframe_test_utils.cpp ../audioframe/audioframe_test_utils.h
)

target_include_directories(vcaudiotriggers_golden_test PRIVATE
    ../../src
    ../../audio/src
    ../../../plugins/interfaces
    ../audioframe
)

target_link_libraries(vcaudiotriggers_golden_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
    qlcplusaudio
)
