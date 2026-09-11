set(module_name "qlcplusaudio")

# Keep dependency options local to avoid changing QLC+'s tests or library types.
function(qlc_add_samplerate)
    include(FetchContent)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)
    set(LIBSAMPLERATE_EXAMPLES OFF)
    set(LIBSAMPLERATE_INSTALL OFF)
    FetchContent_Declare(qlc_samplerate
        GIT_REPOSITORY https://github.com/libsndfile/libsamplerate.git
        GIT_TAG c96f5e3de9c4488f4e6c97f59f5245f22fda22f7
    )
    FetchContent_MakeAvailable(qlc_samplerate)
    set_property(TARGET samplerate PROPERTY POSITION_INDEPENDENT_CODE ON)
    install(FILES "${qlc_samplerate_SOURCE_DIR}/COPYING"
        DESTINATION share/qlcplus/licenses RENAME libsamplerate-COPYING)
endfunction()
qlc_add_samplerate()

add_library(${module_name}
    STATIC
    audiochannel.cpp audiochannel.h
    audiochannelconfig.cpp audiochannelconfig.h
    audioanalyzer.cpp audioanalyzer.h
    ../../src/audioview.cpp ../../src/audioview.h
    audio.cpp audio.h
    audiocapture.cpp audiocapture.h
    beattracker.cpp beattracker.h
    audioframe.h
    audiosnapshot.h
    audiodecoder.cpp audiodecoder.h
    audioparameters.cpp audioparameters.h
    audioplugincache.cpp audioplugincache.h
    audiorenderer.cpp audiorenderer.h
    aubioprocessor.cpp aubioprocessor.h
    aubioresults.h
    melpostprocessor.cpp melpostprocessor.h
    oscaudiosource.cpp oscaudiosource.h
)
set_property(TARGET ${module_name} PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(${module_name} PUBLIC
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(${module_name} PUBLIC
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Multimedia
    Qt${QT_MAJOR_VERSION}::Network
    qlcplusaubio
    SampleRate::samplerate
)

if(APPLE)
    set_target_properties(${module_name} PROPERTIES
        MACOSX_BUNDLE FALSE
    )
endif()

if(WIN32)
    target_link_libraries(${module_name} PUBLIC
        Qt${QT_MAJOR_VERSION}::Widgets
    )
endif()

if(((QT_VERSION_MAJOR LESS 5)) AND (UNIX AND NOT APPLE))
    target_sources(${module_name} PRIVATE
        audiocapture_alsa.cpp audiocapture_alsa.h
        audiorenderer_alsa.cpp audiorenderer_alsa.h
    )
endif()

if(((QT_VERSION_MAJOR LESS 5)) AND (WIN32))
    target_sources(${module_name} PRIVATE
        audiocapture_wavein.cpp audiocapture_wavein.h
        audiorenderer_waveout.cpp audiorenderer_waveout.h
    )
endif()

if((QT_VERSION_MAJOR LESS 6))
    target_sources(${module_name} PRIVATE
        audiocapture_qt5.cpp audiocapture_qt5.h
        audiorenderer_qt5.cpp audiorenderer_qt5.h
    )
endif()

if(NOT ((QT_VERSION_MAJOR LESS 6)))
    target_sources(${module_name} PRIVATE
        audiocapture_qt6.cpp audiocapture_qt6.h
        audiorenderer_qt6.cpp audiorenderer_qt6.h
    )
endif()

if((((QT_VERSION_MAJOR LESS 5)) AND (APPLE)))
    if (${PORTAUDIO_2_FOUND})
        target_sources(${module_name} PRIVATE
            audiocapture_portaudio.cpp audiocapture_portaudio.h
            audiorenderer_portaudio.cpp audiorenderer_portaudio.h
        )

        target_compile_definitions(${module_name} PUBLIC
            HAS_PORTAUDIO
        )

        target_include_directories(${module_name} PUBLIC
            ${PORTAUDIO_2_INCLUDE_DIRS}
        )

        target_link_libraries(${module_name} PUBLIC
            ${PORTAUDIO_2_LIBRARIES}
        )
    endif()
endif()

if(UNIX AND NOT ANDROID AND NOT IOS AND NOT APPLE)
    target_link_libraries(${module_name} PUBLIC
        asound
    )
endif()
