add_executable(audiochannel_test WIN32
    audiochannel_test.cpp audiochannel_test.h
    ../audioframe/audioframe_test_utils.cpp ../audioframe/audioframe_test_utils.h
)

target_include_directories(audiochannel_test PRIVATE
    ../../audio/src
    ../audioframe
)

target_link_libraries(audiochannel_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusaudio
)
