add_executable(audioanalyzer_test WIN32
    audioanalyzer_test.cpp audioanalyzer_test.h
    ../audioframe/audioframe_test_utils.cpp ../audioframe/audioframe_test_utils.h
)

target_include_directories(audioanalyzer_test PRIVATE
    ../../audio/src
    ../audioframe
)

target_link_libraries(audioanalyzer_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusaudio
)
