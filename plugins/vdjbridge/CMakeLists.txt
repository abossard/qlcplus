set(module_name "vdjbridge")

add_library(${module_name} SHARED)

target_sources(${module_name} PRIVATE
    ../interfaces/qlcioplugin.cpp ../interfaces/qlcioplugin.h
    vdjbridgeplugin.cpp vdjbridgeplugin.h
    vdjtelemetryclient.cpp vdjtelemetryclient.h
    vdjbonjour.cpp vdjbonjour.h
)

target_include_directories(${module_name} PRIVATE
    ../interfaces
)

target_link_libraries(${module_name} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Network
    Qt${QT_MAJOR_VERSION}::Widgets
)

# macOS: CoreServices provides dns_sd APIs used by VdjBonjour.
if(APPLE)
    target_link_libraries(${module_name} PRIVATE "-framework CoreServices")
endif()

install(TARGETS ${module_name}
    LIBRARY DESTINATION ${INSTALLROOT}/${PLUGINDIR}
    RUNTIME DESTINATION ${INSTALLROOT}/${PLUGINDIR}
)
