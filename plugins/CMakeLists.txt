project(plugins)

# All plugin .dylib/.so/.dll files must end up in the flat directory that
# IOPluginCache scans at runtime (e.g. build/PlugIns on macOS).
# Setting LIBRARY_OUTPUT_DIRECTORY on each target avoids the need for
# manual symlinks during development.
set(PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/${PLUGINDIR}")

# libusb-1.0
pkg_check_modules(LIBUSB_1 IMPORTED_TARGET libusb-1.0)

add_subdirectory(artnet)
add_subdirectory(ddp)
add_subdirectory(E1.31)
add_subdirectory(loopback)
add_subdirectory(osc)
add_subdirectory(os2l)

# Collect targets that are always built
set(_plugin_targets artnet ddp e131 loopback osc os2l)

if(NOT ANDROID AND NOT IOS)
    pkg_check_modules(LIBOLA IMPORTED_TARGET libola)
    pkg_check_modules(LIBOLASERVER IMPORTED_TARGET libolaserver)

    add_subdirectory(dmxusb)
    add_subdirectory(peperoni)
    add_subdirectory(udmx)
    add_subdirectory(midi)
    add_subdirectory(enttecwing)
    add_subdirectory(hid)
    list(APPEND _plugin_targets dmxusb peperoni udmx midiplugin enttecwing hidplugin)
if(UNIX AND NOT APPLE)
    add_subdirectory(dmx4linux)
    list(APPEND _plugin_targets dmx4linux)
endif()
if(NOT WIN32)
    add_subdirectory(velleman)
    list(APPEND _plugin_targets velleman)
endif()
if(NOT WIN32 AND NOT APPLE)
    add_subdirectory(spi)
    list(APPEND _plugin_targets spi)
endif()
if(UNIX AND ${LIBOLA_FOUND} AND ${LIBOLASERVER_FOUND})
    add_subdirectory(ola)
    list(APPEND _plugin_targets olaio)
endif()

#    add_subdirectory(uart)
#    add_subdirectory(gpio)
endif()

# Point all plugin libraries to the flat scan directory
foreach(_target IN LISTS _plugin_targets)
    if(TARGET ${_target})
        set_target_properties(${_target} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
        )
    endif()
endforeach()
