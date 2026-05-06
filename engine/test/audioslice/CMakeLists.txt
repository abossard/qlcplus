add_executable(audioslice_test WIN32
    audioslice_test.cpp audioslice_test.h
    ../audioframe/audioframe_test_utils.cpp ../audioframe/audioframe_test_utils.h
)

target_include_directories(audioslice_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../../audio/src
    ../audioframe
)

target_compile_definitions(audioslice_test PRIVATE
    RGBSCRIPTV4_SOURCE_PATH="${CMAKE_SOURCE_DIR}/engine/src/rgbscriptv4.cpp"
)

target_link_libraries(audioslice_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
    qlcplusaudio
)

if(qmlui OR (QT_VERSION_MAJOR GREATER 5))
    target_link_libraries(audioslice_test PRIVATE
        Qt${QT_MAJOR_VERSION}::Qml
    )
endif()

if(NOT (qmlui OR (QT_VERSION_MAJOR GREATER 5)))
    target_link_libraries(audioslice_test PRIVATE
        Qt${QT_MAJOR_VERSION}::Script
    )
endif()
