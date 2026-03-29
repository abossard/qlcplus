project(modtemplates)

file(GLOB MODTEMPLATE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.qxmt")

# Only perform the copy if we're building for Android
if(ANDROID)
    # Define the destination directory for the modtemplate files within the Android package
    set(MODTEMPLATES_ASSETS_DIR "${QT_ANDROID_PACKAGE_SOURCE_DIR}/${MODIFIERSTEMPLATEDIR}")

    # Create the modtemplates assets directory if it doesn't exist
    file(MAKE_DIRECTORY ${MODTEMPLATES_ASSETS_DIR})

    # Copy the modtemplate .qxmt files to the assets directory
    foreach(MODTEMPLATE_FILE IN LISTS MODTEMPLATE_FILES)
        file(COPY ${MODTEMPLATE_FILE} DESTINATION ${MODTEMPLATES_ASSETS_DIR})
    endforeach()
endif()

# Create symlinks in build directory for dev builds
if(NOT ANDROID)
    foreach(MODTEMPLATE_FILE IN LISTS MODTEMPLATE_FILES)
        get_filename_component(FILENAME ${MODTEMPLATE_FILE} NAME)
        if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}")
            file(CREATE_LINK ${MODTEMPLATE_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME} SYMBOLIC)
        endif()
    endforeach()
endif()

install(FILES ${MODTEMPLATE_FILES} DESTINATION ${INSTALLROOT}/${MODIFIERSTEMPLATEDIR})
