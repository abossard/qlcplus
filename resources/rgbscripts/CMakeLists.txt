project(scripts)

file(GLOB SCRIPT_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.js")
list(REMOVE_ITEM SCRIPT_FILES "empty.js")

# The engine tests resolve INTERNAL_SCRIPTDIR / INTERNAL_HUESCRIPTDIR relative to
# their own build directory ("../../../resources/rgbscripts/"), so the scripts have to
# exist inside the build tree or rgbscript, rgbmatrix, rgbalgorithm and huematrix
# all fail in initTestCase. Nothing else populated it, so a fresh clone could not
# run the suite. rgbscript_test also opens this CMakeLists.txt to check every
# script is listed for delivery, so copy the file itself too -- and writable,
# because it opens it ReadWrite.
set(SCRIPTS_BUILD_DIR "${CMAKE_BINARY_DIR}/resources/rgbscripts")
file(MAKE_DIRECTORY ${SCRIPTS_BUILD_DIR})
file(COPY ${SCRIPT_FILES} CMakeLists.txt
     DESTINATION ${SCRIPTS_BUILD_DIR}
     FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

# Only perform the copy if we're building for Android
if(ANDROID)
    # Define the destination directory for the script files within the Android package
    set(SCRIPTS_ASSETS_DIR "${QT_ANDROID_PACKAGE_SOURCE_DIR}/${INSTALLROOT}/${RGBSCRIPTDIR}")

    # Create the scripts assets directory if it doesn't exist
    file(MAKE_DIRECTORY ${SCRIPTS_ASSETS_DIR})

    # Copy the script files to the assets directory
    foreach(SCRIPT_FILE IN LISTS SCRIPT_FILES)
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT_FILE} DESTINATION ${SCRIPTS_ASSETS_DIR})
    endforeach()
endif()

install(FILES ${SCRIPT_FILES} DESTINATION ${INSTALLROOT}/${RGBSCRIPTDIR})
