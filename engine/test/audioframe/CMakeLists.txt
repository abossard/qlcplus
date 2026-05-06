add_executable(audioframe_test WIN32
    audioframe_test.cpp audioframe_test.h
    audioframe_test_utils.cpp audioframe_test_utils.h
)

target_include_directories(audioframe_test PRIVATE
    ../../audio/src
)

target_link_libraries(audioframe_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusaudio
)
