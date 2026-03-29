project(colorfilters)

file(GLOB COLOR_FILTER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.qxcf")

if(ANDROID)
    set(COLOR_FILTERS_ASSETS_DIR "${QT_ANDROID_PACKAGE_SOURCE_DIR}/${COLORFILTERSDIR}")
    # Create the assets directory if it doesn't exist
    file(MAKE_DIRECTORY ${COLOR_FILTERS_ASSETS_DIR})

    # Copy the color filter files to the assets directory
    foreach(COLOR_FILTER_FILE ${COLOR_FILTER_FILES})
        file(COPY ${COLOR_FILTER_FILE} DESTINATION ${COLOR_FILTERS_ASSETS_DIR})
    endforeach()
endif()

# Create symlinks in build directory for dev builds (out-of-source only)
get_filename_component(_src_real "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)
get_filename_component(_bld_real "${CMAKE_CURRENT_BINARY_DIR}" REALPATH)
if(NOT ANDROID AND NOT _src_real STREQUAL _bld_real)
    foreach(COLOR_FILTER_FILE ${COLOR_FILTER_FILES})
        get_filename_component(FILENAME ${COLOR_FILTER_FILE} NAME)
        if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}")
            file(CREATE_LINK ${COLOR_FILTER_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME} SYMBOLIC)
        endif()
    endforeach()
endif()

install(FILES ${COLOR_FILTER_FILES} DESTINATION ${INSTALLROOT}/${COLORFILTERSDIR})
