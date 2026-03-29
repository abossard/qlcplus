project(generic_meshes)

file(GLOB GENERIC_MESH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.obj")

# Only perform the copy if we're building for Android
if(ANDROID)
    # Define the destination directory for the generic mesh files within the Android package
    set(GENERIC_MESHES_ASSETS_DIR "${QT_ANDROID_PACKAGE_SOURCE_DIR}/${MESHESDIR}/generic")

    # Create the generic meshes assets directory if it doesn't exist
    file(MAKE_DIRECTORY ${GENERIC_MESHES_ASSETS_DIR})

    # Copy the generic mesh .obj files to the assets directory
    foreach(GENERIC_MESH_FILE IN LISTS GENERIC_MESH_FILES)
        file(COPY ${GENERIC_MESH_FILE} DESTINATION ${GENERIC_MESHES_ASSETS_DIR})
    endforeach()
endif()

# Create symlinks in build directory for dev builds
if(NOT ANDROID)
    foreach(GENERIC_MESH_FILE IN LISTS GENERIC_MESH_FILES)
        get_filename_component(FILENAME ${GENERIC_MESH_FILE} NAME)
        if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}")
            file(CREATE_LINK ${GENERIC_MESH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME} SYMBOLIC)
        endif()
    endforeach()
endif()

install(FILES ${GENERIC_MESH_FILES} DESTINATION ${INSTALLROOT}/${MESHESDIR}/generic)
