add_executable(aubio_smoke_test aubio_smoke_test.cpp)
target_link_libraries(aubio_smoke_test PRIVATE qlcplusaubio)

add_executable(osc_audio_source_test osc_audio_source_test.cpp)
target_link_libraries(osc_audio_source_test PRIVATE
    qlcplusengine
    qlcplusaudio
    Qt${QT_MAJOR_VERSION}::Test
    Qt${QT_MAJOR_VERSION}::Network
)
target_include_directories(osc_audio_source_test PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/audio/src
    ${CMAKE_SOURCE_DIR}/engine/src
)
