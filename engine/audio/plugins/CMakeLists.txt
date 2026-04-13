project(plugins)

set(AUDIO_PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/${AUDIOPLUGINDIR}")

if((((NOT ANDROID AND NOT IOS))))
#    pkg_check_modules(MAD IMPORTED_TARGET mad)
#    if(${MAD_FOUND})
#        add_subdirectory(mad)
#    endif()

    pkg_check_modules(SNDFILE IMPORTED_TARGET sndfile)
    if(${SNDFILE_FOUND})
        add_subdirectory(sndfile)
        if(TARGET sndfileplugin)
            set_target_properties(sndfileplugin PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY "${AUDIO_PLUGIN_OUTPUT_DIR}"
                RUNTIME_OUTPUT_DIRECTORY "${AUDIO_PLUGIN_OUTPUT_DIR}"
            )
        endif()
    endif()
endif()
