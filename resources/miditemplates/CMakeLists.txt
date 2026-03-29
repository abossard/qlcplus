project(miditemplates)

file(GLOB MIDITEMPLATE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.qxm")

# Only perform the copy if we're building for Android
if(ANDROID)
    # Define the destination directory for the miditemplate files within the Android package
    set(MIDITEMPLATES_ASSETS_DIR "${QT_ANDROID_PACKAGE_SOURCE_DIR}/${MIDITEMPLATEDIR}")

    # Create the miditemplates assets directory if it doesn't exist
    file(MAKE_DIRECTORY ${MIDITEMPLATES_ASSETS_DIR})

    # Copy the miditemplate .qxm files to the assets directory
    foreach(MIDITEMPLATE_FILE IN LISTS MIDITEMPLATE_FILES)
        file(COPY ${MIDITEMPLATE_FILE} DESTINATION ${MIDITEMPLATES_ASSETS_DIR})
    endforeach()
endif()

# Create symlinks in build directory for dev builds (out-of-source only)
get_filename_component(_src_real "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)
get_filename_component(_bld_real "${CMAKE_CURRENT_BINARY_DIR}" REALPATH)
if(NOT ANDROID AND NOT _src_real STREQUAL _bld_real)
    foreach(MIDITEMPLATE_FILE IN LISTS MIDITEMPLATE_FILES)
        get_filename_component(FILENAME ${MIDITEMPLATE_FILE} NAME)
        if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}")
            file(CREATE_LINK ${MIDITEMPLATE_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME} SYMBOLIC)
        endif()
    endforeach()
endif()

install(FILES ${MIDITEMPLATE_FILES} DESTINATION ${INSTALLROOT}/${MIDITEMPLATEDIR})
