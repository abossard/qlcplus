set(module_name "ddp")

add_library(${module_name}
    SHARED
)

target_sources(${module_name} PRIVATE
    ../interfaces/qlcioplugin.cpp ../interfaces/qlcioplugin.h
    configureddp.cpp configureddp.h configureddp.ui
    ddpcontroller.cpp ddpcontroller.h
    ddppacketizer.cpp ddppacketizer.h
    ddpplugin.cpp ddpplugin.h
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

install(TARGETS ${module_name}
    LIBRARY DESTINATION ${INSTALLROOT}/${PLUGINDIR}
    RUNTIME DESTINATION ${INSTALLROOT}/${PLUGINDIR}
)

if(NOT ANDROID AND NOT IOS)
    add_subdirectory(test)
endif()
